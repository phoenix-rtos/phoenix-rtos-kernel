/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Process coredump
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jakub Klimek
 *
 * %LICENSE%
 */

#include "coredump.h"
#include "arch/cpu.h"
#include "arch/exceptions.h"
#include "hal/exceptions.h"
#include "log/log.h"
#include "proc/elf.h"
#include "proc/process.h"
#include "lib/encoding.h"
#include <stddef.h>

#ifndef PROC_COREDUMP

void coredump_dump(unsigned int n, exc_context_t *ctx)
{
	return;
}

#else

#define COREDUMP_OUTBUF_SIZE 128

#define MEM_NONE       0
#define MEM_EXC_STACK  1
#define MEM_ALL_STACKS 2
#define MEM_ALL        3

#ifndef PROC_COREDUMP_MEM_OPT
#define PROC_COREDUMP_MEM_OPT MEM_EXC_STACK
#endif

#ifndef PROC_COREDUMP_THREADS_NUM
#define PROC_COREDUMP_THREADS_NUM 1
#endif

#define CORE_BUF_SIZE_MAX max(SIZE_COREDUMP_GREGSET, max(SIZE_COREDUMP_THREADAUX, SIZE_COREDUMP_GENAUX))

#define COREDUMP_START "\n_____________COREDUMP_START_____________\n"
#define COREDUMP_END   "\n______________COREDUMP_END______________\n"

#define PRSTATUS_NAME "CORE"

typedef struct {
	struct elf_siginfo {
		int si_signo;
		int si_code;
		int si_errno;
	} pr_info;
	short pr_cursig;
	unsigned long pr_sigpend;
	unsigned long pr_sighold;
	pid_t pr_pid;
	pid_t pr_ppid;
	pid_t pr_pgrp;
	pid_t pr_sid;
	struct timeval {
		long tv_sec;
		long tv_usec;
	} pr_utime;
	struct timeval pr_stime;
	struct timeval pr_cutime;
	struct timeval pr_cstime;
	char pr_reg[0]; /* actual size depends on architecture */
	int pr_fpvalid;
} elf_prstatus;

typedef struct {
	char outBuf[COREDUMP_OUTBUF_SIZE];
	size_t outCur;

	u8 rle_last;
	size_t rle_count;

	lib_base64_ctx b64;
	crc32_t crc32;
} coredump_state_t;


static void coredump_write(const char *data, size_t len)
{
#ifdef PROC_COREDUMP_WRITE_SERIAL
	hal_consolePrint(ATTR_NORMAL, data);
#endif
#ifdef PROC_COREDUMP_WRITE_LOG
	log_write(data, len);
#endif
}


static void coredump_writeBuf(coredump_state_t *state, const char *data, size_t len)
{
	while (state->outCur + len >= sizeof(state->outBuf) - 1) {
		hal_memcpy(state->outBuf + state->outCur, data, sizeof(state->outBuf) - 1 - state->outCur);
		state->outBuf[sizeof(state->outBuf) - 1] = '\0';
		coredump_write(state->outBuf, sizeof(state->outBuf));
		data += sizeof(state->outBuf) - 1 - state->outCur;
		len -= sizeof(state->outBuf) - 1 - state->outCur;
		state->outCur = 0;
	}
	hal_memcpy(state->outBuf + state->outCur, data, len);
	state->outCur += len;
	if (state->outCur >= sizeof(state->outBuf) - 1) {
		state->outBuf[sizeof(state->outBuf) - 1] = '\0';
		coredump_write(state->outBuf, sizeof(state->outBuf));
		state->outCur = 0;
	}
}


static void coredump_nextByte(coredump_state_t *state, const u8 byte)
{
	int n = lib_base64EncodeByte(&state->b64, byte);
	coredump_writeBuf(state, state->b64.outBuf, n);
}


static void coredump_encodeRleLength(coredump_state_t *state)
{
	u8 byte;
	while (state->rle_count > 0) {
		byte = state->rle_count & 0x7F;
		state->rle_count >>= 7;
		if (state->rle_count > 0) {
			byte |= 0x80;
		}
		coredump_nextByte(state, byte);
	}
}


static void coredump_init(coredump_state_t *state, const char *path, const char *mnemonic)
{
	state->outCur = 0;
	state->rle_last = -1;
	state->rle_count = 0;
	state->crc32 = LIB_CRC32_INIT;
	lib_base64Init(&state->b64);

	coredump_write(COREDUMP_START, sizeof(COREDUMP_START) - 1);
	coredump_write(path, hal_strlen(path));
	coredump_write(": ", 2);
	coredump_write(mnemonic, hal_strlen(mnemonic));
	coredump_write(";\n", 2);
}


static void coredump_encodeChunk(coredump_state_t *state, const u8 *buf, size_t len)
{
	size_t i;
	u8 byte;
	for (i = 0; i < len; i++) {
		/* making sure that crc is coherent with dumped data even if the buf is written to by other process */
		byte = buf[i];
		state->crc32 = lib_crc32NextByte(state->crc32, byte);

		if (state->rle_last == byte) {
			state->rle_count++;
			continue;
		}
		if ((state->rle_count > 3) || ((state->rle_last == 0xFE) && (state->rle_count > 0))) {
			coredump_nextByte(state, 0xFE);
			coredump_encodeRleLength(state);
			coredump_nextByte(state, state->rle_last);
		}
		else {
			while (state->rle_count > 0) {
				coredump_nextByte(state, state->rle_last);
				state->rle_count--;
			}
		}
		state->rle_count = 1;
		state->rle_last = byte;
	}
}


static void coredump_finalize(coredump_state_t *state)
{
	crc32_t crc = lib_crc32Finalize(state->crc32);
	coredump_encodeChunk(state, (u8 *)&crc, sizeof(crc));

	if ((state->rle_count > 3) || (state->rle_last == 0xFE)) {
		coredump_nextByte(state, 0xFE);
		coredump_encodeRleLength(state);
		coredump_nextByte(state, state->rle_last);
	}
	else {
		while (state->rle_count > 0) {
			coredump_nextByte(state, state->rle_last);
			state->rle_count--;
		}
	}

	int n = lib_base64Finalize(&state->b64);
	coredump_writeBuf(state, state->b64.outBuf, n);


	if (state->outCur > 0) {
		state->outBuf[state->outCur++] = '\0';
		coredump_write(state->outBuf, state->outCur);
	}

	coredump_write(COREDUMP_END, sizeof(COREDUMP_END) - 1);
}


static int isElfClass32(coredump_state_t *state)
{
	return sizeof(void *) == 8;
}


static void coredump_dumpElfHeader32(size_t segCnt, coredump_state_t *state)
{
	Elf32_Ehdr hdr;

	hal_memcpy(hdr.e_ident, ELFMAG, sizeof(ELFMAG));
	hal_memset(hdr.e_ident + sizeof(ELFMAG), 0, sizeof(hdr.e_ident) - sizeof(ELFMAG));
	hdr.e_ident[EI_CLASS] = ELFCLASS32;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	hdr.e_ident[EI_DATA] = ELFDATA2LSB;
#else
	hdr.e_ident[EI_DATA] = ELFDATA2MSB;
#endif
	hdr.e_ident[EI_VERSION] = 1; /* EV_CURRENT */
	hdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
	hdr.e_type = ET_CORE;
	hdr.e_machine = HAL_ELF_MACHINE;
	hdr.e_version = 1; /* EV_CURRENT */
	hdr.e_phoff = sizeof(Elf32_Ehdr);
	hdr.e_ehsize = sizeof(Elf32_Ehdr);
	hdr.e_phentsize = sizeof(Elf32_Phdr);
	hdr.e_phnum = 1 + segCnt;
	hdr.e_shoff = 0;
	hdr.e_flags = 0;
	hdr.e_shentsize = 0;
	hdr.e_shnum = 0;
	hdr.e_shstrndx = 0;
	hdr.e_entry = 0;

	coredump_encodeChunk(state, (u8 *)&hdr, sizeof(hdr));
}


static void coredump_dumpElfHeader64(size_t segCnt, coredump_state_t *state)
{
	Elf64_Ehdr hdr;

	hal_memcpy(hdr.e_ident, ELFMAG, sizeof(ELFMAG));
	hal_memset(hdr.e_ident + sizeof(ELFMAG), 0, sizeof(hdr.e_ident) - sizeof(ELFMAG));
	hdr.e_ident[EI_CLASS] = ELFCLASS64;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	hdr.e_ident[EI_DATA] = ELFDATA2LSB;
#else
	hdr.e_ident[EI_DATA] = ELFDATA2MSB;
#endif
	hdr.e_ident[EI_VERSION] = 1; /* EV_CURRENT */
	hdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
	hdr.e_type = ET_CORE;
	hdr.e_machine = HAL_ELF_MACHINE;
	hdr.e_version = 1; /* EV_CURRENT */
	hdr.e_phoff = sizeof(Elf64_Ehdr);
	hdr.e_ehsize = sizeof(Elf64_Ehdr);
	hdr.e_phentsize = sizeof(Elf64_Phdr);
	hdr.e_phnum = 1 + segCnt;
	hdr.e_shoff = 0;
	hdr.e_flags = 0;
	hdr.e_shentsize = 0;
	hdr.e_shnum = 0;
	hdr.e_shstrndx = 0;
	hdr.e_entry = 0;

	coredump_encodeChunk(state, (u8 *)&hdr, sizeof(hdr));
}


static void coredump_dumpElfHeader(size_t segCnt, coredump_state_t *state)
{
	if (isElfClass32(state)) {
		coredump_dumpElfHeader32(segCnt, state);
	}
	else {
		coredump_dumpElfHeader64(segCnt, state);
	}
}


static size_t align4(size_t size)
{
	return (size + 3) & ~3;
}


static void coredump_dumpThreadNotes(coredump_threadinfo_t *threads, size_t threadCnt, coredump_state_t *state, void *buff)
{
	const u32 zero = 0;
	Elf32_Nhdr nhdr; /* Elf64_Nhdr is identical to Elf32_Nhdr */
	size_t i;
	elf_prstatus prstatus;
	hal_memset(&prstatus, 0, sizeof(prstatus));
	for (i = 0; i < threadCnt; i++) {
		nhdr.n_namesz = sizeof(PRSTATUS_NAME);
		nhdr.n_descsz = sizeof(elf_prstatus) + SIZE_COREDUMP_GREGSET;
		nhdr.n_type = NT_PRSTATUS;
		coredump_encodeChunk(state, (u8 *)&nhdr, sizeof(nhdr));
		coredump_encodeChunk(state, (u8 *)PRSTATUS_NAME, sizeof(PRSTATUS_NAME));
		/* alignment */
		coredump_encodeChunk(state, (u8 *)&zero, align4(sizeof(PRSTATUS_NAME)) - sizeof(PRSTATUS_NAME));

		prstatus.pr_pid = threads[i].tid;
		coredump_encodeChunk(state, (u8 *)&prstatus, offsetof(elf_prstatus, pr_reg));
		hal_coredumpGRegset(buff, threads[i].userContext);
		coredump_encodeChunk(state, (u8 *)buff, SIZE_COREDUMP_GREGSET);
		coredump_encodeChunk(state, (u8 *)&prstatus.pr_reg, sizeof(prstatus) - offsetof(elf_prstatus, pr_reg));

		hal_coredumpThreadAux(buff, threads[i].userContext);
		coredump_encodeChunk(state, (u8 *)buff, SIZE_COREDUMP_THREADAUX);
	}
}


static size_t coredump_stackSize(void *userSp, process_t *process)
{
	map_entry_t t;
	map_entry_t *e;
	size_t stackSize;

	t.vaddr = userSp;
	t.size = 1;

	proc_lockSet(&process->mapp->lock);

	e = lib_treeof(map_entry_t, linkage, lib_rbFind(&process->mapp->tree, &t.linkage));
	if (e == NULL) {
		proc_lockClear(&process->mapp->lock);
		return 0;
	}
	stackSize = e->vaddr + e->size - userSp;

	proc_lockClear(&process->mapp->lock);

	return stackSize;
}


static void coredump_dumpStack(process_t *process, cpu_context_t *ctx, coredump_state_t *state)
{
	void *userSp;
	size_t stackSize;

	userSp = hal_cpuGetUserSP(ctx);
	stackSize = coredump_stackSize(userSp, process);

	coredump_encodeChunk(state, userSp, stackSize);
}


static void coredump_dumpPhdr32(u32 type, size_t offset, void *vaddr, size_t size, unsigned short flags, coredump_state_t *state)
{
	Elf32_Phdr phdr;
	phdr.p_type = type;
	phdr.p_offset = offset;
	phdr.p_vaddr = (Elf32_Addr)(ptr_t)vaddr;
	phdr.p_paddr = 0;
	phdr.p_filesz = size;
	phdr.p_memsz = size;

	phdr.p_flags = 0;
	if (flags & PROT_READ) {
		phdr.p_flags |= PF_R;
	}
	if (flags & PROT_WRITE) {
		phdr.p_flags |= PF_W;
	}
	if (flags & PROT_EXEC) {
		phdr.p_flags |= PF_X;
	}

	phdr.p_align = 0;
	coredump_encodeChunk(state, (u8 *)&phdr, sizeof(phdr));
}


static void coredump_dumpPhdr64(u32 type, size_t offset, void *vaddr, size_t size, unsigned short flags, coredump_state_t *state)
{
	Elf64_Phdr phdr;
	phdr.p_type = type;
	phdr.p_offset = offset;
	phdr.p_vaddr = (Elf64_Addr)(ptr_t)vaddr;
	phdr.p_paddr = 0;
	phdr.p_filesz = size;
	phdr.p_memsz = size;

	phdr.p_flags = 0;
	if (flags & PROT_READ) {
		phdr.p_flags |= PF_R;
	}
	if (flags & PROT_WRITE) {
		phdr.p_flags |= PF_W;
	}
	if (flags & PROT_EXEC) {
		phdr.p_flags |= PF_X;
	}

	phdr.p_align = 0;
	coredump_encodeChunk(state, (u8 *)&phdr, sizeof(phdr));
}


static void coredump_dumpPhdr(u32 type, size_t offset, void *vaddr, size_t size, unsigned short flags, coredump_state_t *state)
{
	if (isElfClass32(state)) {
		coredump_dumpPhdr32(type, offset, vaddr, size, flags, state);
	}
	else {
		coredump_dumpPhdr64(type, offset, vaddr, size, flags, state);
	}
}


static void coredump_dumpAllPhdrs(coredump_threadinfo_t *threadInfo, size_t threadCnt, size_t segCnt, process_t *process, coredump_state_t *state)
{
	static const size_t THREAD_NOTES_SIZE = sizeof(Elf32_Nhdr) +
			((sizeof(PRSTATUS_NAME) + 3) & ~3) +
			sizeof(elf_prstatus) +
			SIZE_COREDUMP_GREGSET +
			SIZE_COREDUMP_THREADAUX;
	const size_t NOTES_SIZE = SIZE_COREDUMP_GENAUX + threadCnt * (THREAD_NOTES_SIZE);
	size_t currentOffset;
	size_t stackSize;
	void *userSp;
	map_entry_t *e;
	size_t i;

	/* Notes */
	if (isElfClass32(state)) {
		currentOffset = sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr) * (1 + segCnt);
	}
	else {
		currentOffset = sizeof(Elf64_Ehdr) + sizeof(Elf64_Phdr) * (1 + segCnt);
	}

	coredump_dumpPhdr(PT_NOTE, currentOffset, 0, NOTES_SIZE, 0, state);
	currentOffset += NOTES_SIZE;

	/* Memory */
	if (PROC_COREDUMP_MEM_OPT == MEM_ALL) {
		proc_lockSet(&process->mapp->lock);
		e = lib_treeof(map_entry_t, linkage, lib_rbMinimum(process->mapp->tree.root));
		while (e != NULL) {
			if ((e->prot & PROT_READ) && (e->prot & PROT_WRITE)) {
				coredump_dumpPhdr(PT_LOAD, currentOffset, e->vaddr, e->size, e->prot, state);
				currentOffset += e->size;
			}
			e = lib_treeof(map_entry_t, linkage, lib_rbNext(&e->linkage));
		}
		proc_lockClear(&process->mapp->lock);
	}
	else if (PROC_COREDUMP_MEM_OPT == MEM_EXC_STACK) {
		/* exception thread is put into threadInfo at index 0 */
		userSp = hal_cpuGetUserSP(threadInfo[0].userContext);
		stackSize = coredump_stackSize(userSp, process);
		coredump_dumpPhdr(PT_LOAD, currentOffset, userSp, stackSize, PROT_READ | PROT_WRITE, state);
		currentOffset += stackSize;
	}
	else if (PROC_COREDUMP_MEM_OPT == MEM_ALL_STACKS) {
		for (i = 0; i < threadCnt; i++) {
			userSp = hal_cpuGetUserSP(threadInfo[i].userContext);
			stackSize = coredump_stackSize(userSp, process);
			coredump_dumpPhdr(PT_LOAD, currentOffset, userSp, stackSize, PROT_READ | PROT_WRITE, state);
			currentOffset += stackSize;
		}
	}
}


static void coredump_dumpAllMemory(process_t *process, coredump_state_t *state)
{
	map_entry_t *e;

	proc_lockSet(&process->mapp->lock);
	e = lib_treeof(map_entry_t, linkage, lib_rbMinimum(process->mapp->tree.root));
	while (e != NULL) {
		if ((e->prot & PROT_READ) && (e->prot & PROT_WRITE)) {
			coredump_encodeChunk(state, e->vaddr, e->size);
		}
		e = lib_treeof(map_entry_t, linkage, lib_rbNext(&e->linkage));
	}
	proc_lockClear(&process->mapp->lock);
}


static size_t coredump_segmentCount(process_t *process)
{
	map_entry_t *e;
	size_t segCnt = 0;

	proc_lockSet(&process->mapp->lock);

	e = lib_treeof(map_entry_t, linkage, lib_rbMinimum(process->mapp->tree.root));
	while (e != NULL) {
		if ((e->prot & PROT_READ) && (e->prot & PROT_WRITE)) {
			segCnt++;
		}
		e = lib_treeof(map_entry_t, linkage, lib_rbNext(&e->linkage));
	}

	proc_lockClear(&process->mapp->lock);
	return segCnt;
}


static void coredump_currentThreadInfo(cpu_context_t *ctx, unsigned int n, coredump_threadinfo_t *info)
{
	thread_t *current = proc_current();

	info->tid = proc_getTid(current);
	info->cursig = n;
	info->userContext = ctx;
}


void coredump_dump(unsigned int n, exc_context_t *ctx)
{
	char buff[CORE_BUF_SIZE_MAX] __attribute__((aligned(8)));
	coredump_threadinfo_t threadInfo[PROC_COREDUMP_THREADS_NUM];
	size_t segCnt;
	size_t threadCnt;
	coredump_state_t state;
	process_t *process;
	size_t i;

	process = proc_current()->process;

	/*
	 * Ensure for dumped process that:
	 * - saved context is coherent with stack memory,
	 * - thread count, section count are fixed,
	 * while the rest of the processes can run freely.
	 */
	proc_freeze(process);

	coredump_currentThreadInfo(hal_excToCpuCtx(ctx), n, &threadInfo[0]);
	threadCnt = 1 + coredump_threadsInfo(process, 1, PROC_COREDUMP_THREADS_NUM - 1, &threadInfo[1]);

	switch (PROC_COREDUMP_MEM_OPT) {
		case MEM_ALL:
			segCnt = coredump_segmentCount(process);
			break;
		case MEM_ALL_STACKS:
			segCnt = threadCnt;
			break;
		case MEM_EXC_STACK:
			segCnt = 1;
			break;
		default:
			segCnt = 0;
			break;
	}

	coredump_init(&state, process->path, hal_exceptionMnemonic(n));
	coredump_dumpElfHeader(segCnt, &state);
	coredump_dumpAllPhdrs(threadInfo, threadCnt, segCnt, process, &state);

	coredump_dumpThreadNotes(threadInfo, threadCnt, &state, buff);
	hal_coredumpGeneralAux(buff);
	coredump_encodeChunk(&state, (u8 *)buff, SIZE_COREDUMP_GENAUX);

	/* MEMORY */
	if (PROC_COREDUMP_MEM_OPT == MEM_ALL) {
		coredump_dumpAllMemory(process, &state);
	}
	else if (PROC_COREDUMP_MEM_OPT == MEM_EXC_STACK) {
		/* exception thread is put into threadInfo at index 0 */
		coredump_dumpStack(process, threadInfo[0].userContext, &state);
	}
	else if (PROC_COREDUMP_MEM_OPT == MEM_ALL_STACKS) {
		for (i = 0; (i < threadCnt) && (i < PROC_COREDUMP_THREADS_NUM); i++) {
			coredump_dumpStack(process, threadInfo[i].userContext, &state);
		}
	}

	coredump_finalize(&state);

	proc_unfreeze(process);
}
#endif /* PROC_COREDUMP */
