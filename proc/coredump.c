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
 * This file is part of Phoenix-RTOS.
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
#include <stddef.h>

#ifdef COREDUMP

#define COREDUMP_OUTBUF_SIZE 128

#ifndef COREDUMP_THREADS_NUM
#define COREDUMP_THREADS_NUM 1
#endif

#define CORE_BUF_SIZE_MAX max(SIZE_COREDUMP_GREGSET, max(SIZE_COREDUMP_THREADAUX, SIZE_COREDUMP_GENAUX))

#define COREDUMP_START "\n_____________COREDUMP_START_____________\n"
#define COREDUMP_END   "\n______________COREDUMP_END______________\n"

#define PRSTATUS_NAME "CORE"

#define NT_LMA      0x00414D4C /* ASCII for "LMA" (load memory address) */
#define NT_LMA_NAME "PHOENIX"

#if __SIZEOF_POINTER__ == 4
typedef Elf32_Ehdr Elf_Ehdr;
typedef Elf32_Phdr Elf_Phdr;
typedef Elf32_Nhdr Elf_Nhdr;
typedef Elf32_Addr Elf_Addr;
#define ELFCLASS ELFCLASS32
#else
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Phdr Elf_Phdr;
typedef Elf64_Nhdr Elf_Nhdr;
typedef Elf64_Addr Elf_Addr;
#define ELFCLASS ELFCLASS64
#endif

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

#define CRC32POLY_LE 0xedb88320
static const char b64_table[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

typedef struct {
	char outBuf[COREDUMP_OUTBUF_SIZE];
	size_t outCur;

	u32 b64_buf;
	int b64_bits;

	u8 rle_last;
	size_t rle_count;

	u32 crc32;
} coredump_state_t;

static void coredump_write(const char *data, size_t len)
{
#ifdef COREDUMP_WRITE_SERIAL
	hal_consolePrint(ATTR_NORMAL, data);
#endif
#ifdef COREDUMP_WRITE_LOG
	log_write(data, len);
#endif
}


static void coredump_writeBuf(coredump_state_t *state, const char *data, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++) {
		state->outBuf[state->outCur++] = data[i];
		if (state->outCur == sizeof(state->outBuf) - 1) {
			state->outBuf[state->outCur] = '\0';
			coredump_write(state->outBuf, state->outCur);
			state->outCur = 0;
		}
	}
}


static void coredump_b64EncodeByte(coredump_state_t *state, const u8 byte)
{
	char c;

	state->b64_buf <<= 8;
	state->b64_buf |= byte;
	state->b64_bits += 8;
	while (state->b64_bits >= 6) {
		state->b64_bits -= 6;
		c = b64_table[(state->b64_buf >> state->b64_bits) & 0x3F];
		coredump_writeBuf(state, &c, 1);
	}
}


static void coredump_b64encodeRleLength(coredump_state_t *state)
{
	u8 byte;
	while (state->rle_count > 0) {
		byte = state->rle_count & 0x7F;
		state->rle_count >>= 7;
		if (state->rle_count > 0) {
			byte |= 0x80;
		}
		coredump_b64EncodeByte(state, byte);
	}
}


static void coredump_init(coredump_state_t *state, const char *path, const char *mnemonic)
{
	char *textBuff;
	size_t i;

	state->outCur = 0;
	state->b64_buf = 0;
	state->b64_bits = 0;
	state->rle_last = -1;
	state->rle_count = 0;
	state->crc32 = -1;

	coredump_write(COREDUMP_START, sizeof(COREDUMP_START) - 1);
	textBuff = vm_kmalloc(hal_strlen(path) + hal_strlen(mnemonic) + 4);
	hal_strcpy(textBuff, path);
	i = hal_strlen(path);
	hal_strcpy(textBuff + i, ": ");
	i += 2;
	hal_strcpy(textBuff + i, mnemonic);
	i += hal_strlen(mnemonic);
	textBuff[i++] = '\n';
	textBuff[i] = '\0';
	coredump_write(textBuff, i);
	vm_kfree(textBuff);
}


static void coredump_encodeChunk(coredump_state_t *state, const u8 *buf, size_t len)
{
	size_t i = 0;
	size_t b;
	while (i < len) {
		state->crc32 = (state->crc32 ^ (buf[i] & 0xFF));
		for (b = 0; b < 8; b++) {
			state->crc32 = (state->crc32 >> 1) ^ ((state->crc32 & 1) ? CRC32POLY_LE : 0);
		}

		if (state->rle_last == buf[i]) {
			state->rle_count++;
			i++;
			continue;
		}
		if ((state->rle_count > 3) || (state->rle_last == 0xFE && state->rle_count > 0)) {
			coredump_b64EncodeByte(state, 0xFE);
			coredump_b64encodeRleLength(state);
			coredump_b64EncodeByte(state, state->rle_last);
		}
		else {
			while (state->rle_count > 0) {
				coredump_b64EncodeByte(state, state->rle_last);
				state->rle_count--;
			}
		}
		state->rle_count = 1;
		state->rle_last = buf[i];
		i++;
	}
}


static void coredump_finalize(coredump_state_t *state)
{
	char c;
	u32 crc;

	crc = ~state->crc32;
	coredump_encodeChunk(state, (u8 *)&crc, sizeof(crc));

	if ((state->rle_count > 3) || (state->rle_last == 0xFE)) {
		coredump_b64EncodeByte(state, 0xFE);
		coredump_b64encodeRleLength(state);
		coredump_b64EncodeByte(state, state->rle_last);
	}
	else {
		while (state->rle_count > 0) {
			coredump_b64EncodeByte(state, state->rle_last);
			state->rle_count--;
		}
	}

	if (state->b64_bits > 0) {
		c = b64_table[(state->b64_buf << (6 - state->b64_bits)) & 0x3F];
		coredump_writeBuf(state, &c, 1);
		if (state->b64_bits == 4) {
			coredump_writeBuf(state, "=", 1);
		}
		else if (state->b64_bits == 2) {
			coredump_writeBuf(state, "==", 2);
		}
	}

	if (state->outCur > 0) {
		state->outBuf[state->outCur] = '\0';
		coredump_write(state->outBuf, state->outCur);
	}

	coredump_write(COREDUMP_END, sizeof(COREDUMP_END) - 1);
}


static void coredump_dumpElfHeader(size_t segCnt, coredump_state_t *state)
{
	Elf_Ehdr hdr;

	hal_memcpy(hdr.e_ident, ELFMAG, sizeof(ELFMAG));
	hal_memset(hdr.e_ident + sizeof(ELFMAG), 0, sizeof(hdr.e_ident) - sizeof(ELFMAG));
	hdr.e_ident[EI_CLASS] = ELFCLASS;
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	hdr.e_ident[EI_DATA] = ELFDATA2LSB;
#else
	hdr.e_ident[EI_DATA] = ELFDATA2MSB;
#endif
	hdr.e_ident[EI_VERSION] = 1; /* EV_CURRENT */
	hdr.e_ident[EI_OSABI] = ELFOSABI_SYSV;
	hdr.e_type = ET_CORE; /* ET_CORE */
	hdr.e_machine = HAL_ELF_MACHINE;
	hdr.e_version = 1; /* EV_CURRENT */
	hdr.e_phoff = sizeof(Elf_Ehdr);
	hdr.e_ehsize = sizeof(Elf_Ehdr);
	hdr.e_phentsize = sizeof(Elf_Phdr);
	hdr.e_phnum = 1 + segCnt;
	hdr.e_shoff = 0;
	hdr.e_flags = 0;
	hdr.e_shentsize = 0;
	hdr.e_shnum = 0;
	hdr.e_shstrndx = 0;
	hdr.e_entry = 0;

	coredump_encodeChunk(state, (u8 *)&hdr, sizeof(hdr));
}


static size_t align4(size_t size)
{
	return (size + 3) & ~3;
}


static void coredump_dumpThreadNotes(coredump_threadinfo_t *threads, size_t threadCnt, coredump_state_t *state, void *buff)
{
	const u32 zero = 0;
	Elf_Nhdr nhdr;
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
		prstatus.pr_cursig = threads[i].cursig;
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


static void coredump_dumpPhdr(u32 type, size_t offset, void *vaddr, size_t size, unsigned short flags, coredump_state_t *state)
{
	Elf_Phdr phdr;
	phdr.p_type = type;
	phdr.p_offset = offset;
	phdr.p_vaddr = (Elf_Addr)vaddr;
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


#ifdef NOMMU
static void coredump_dumpLMA(process_t *process, coredump_state_t *state)
{
	int i;
	Elf32_Nhdr nhdr = {
		.n_type = NT_LMA,
		.n_namesz = sizeof(NT_LMA_NAME),
		.n_descsz = process->relocsz * 2 * sizeof(Elf32_Addr)
	};
	coredump_encodeChunk(state, (u8 *)&nhdr, sizeof(nhdr));
	coredump_encodeChunk(state, (u8 *)NT_LMA_NAME, sizeof(NT_LMA_NAME));

	for (i = 0; i < process->relocsz; i++) {
		coredump_encodeChunk(state, (u8 *)&process->reloc[i].pbase, sizeof(process->reloc[i].pbase));
		coredump_encodeChunk(state, (u8 *)&process->reloc[i].vbase, sizeof(process->reloc[i].vbase));
	}
}
#endif


static void coredump_dumpPhdrs(coredump_threadinfo_t *threadInfo, size_t threadCnt, size_t segCnt, process_t *process, cpu_context_t *ctx, coredump_state_t *state)
{
	const size_t THREAD_NOTES_SIZE = sizeof(Elf_Nhdr) +
			align4(sizeof(PRSTATUS_NAME)) +
			sizeof(elf_prstatus) +
			SIZE_COREDUMP_GREGSET +
			SIZE_COREDUMP_THREADAUX;
#ifdef NOMMU
	const size_t LMA_NOTE_SIZE = (sizeof(Elf32_Nhdr) + align4(sizeof(NT_LMA_NAME)) +
			process->relocsz * 2 * sizeof(Elf32_Addr));
#else
	const size_t LMA_NOTE_SIZE = 0;
#endif /* NOMMU */
	const size_t NOTES_SIZE = SIZE_COREDUMP_GENAUX + threadCnt * (THREAD_NOTES_SIZE) + LMA_NOTE_SIZE;
	size_t currentOffset;
	size_t stackSize;
	void *userSp;
	map_entry_t *e;
	size_t i;

	/* Notes */
	currentOffset = sizeof(Elf_Ehdr) + sizeof(Elf_Phdr) * (1 + segCnt);
	coredump_dumpPhdr(PT_NOTE, currentOffset, 0, NOTES_SIZE, 0, state);
	currentOffset += NOTES_SIZE;

	/* Memory */
#ifdef COREDUMP_MEM_ALL
	proc_lockSet(&process->mapp->lock);
#ifdef NOMMU
	e = process->entries;
	do {
		if ((e->prot & PROT_READ) && (e->prot & PROT_WRITE)) {
			coredump_dumpPhdr(PT_LOAD, currentOffset, e->vaddr, e->size, e->prot, state);
			currentOffset += e->size;
		}

		e = e->next;
	} while (e != process->entries);
#else
	e = lib_treeof(map_entry_t, linkage, lib_rbMinimum(process->mapp->tree.root));
	while (e != NULL) {
		if ((e->prot & PROT_READ) && (e->prot & PROT_WRITE)) {
			coredump_dumpPhdr(PT_LOAD, currentOffset, e->vaddr, e->size, e->prot, state);
			currentOffset += e->size;
		}
		e = lib_treeof(map_entry_t, linkage, lib_rbNext(&e->linkage));
	}
#endif /* NOMMU */
	proc_lockClear(&process->mapp->lock);
#else
	userSp = hal_cpuGetUserSP(ctx);
	stackSize = coredump_stackSize(userSp, process);
	coredump_dumpPhdr(PT_LOAD, currentOffset, userSp, stackSize, PROT_READ | PROT_WRITE, state);
	currentOffset += stackSize;

#ifdef COREDUMP_MEM_ALL_STACKS
	for (i = 1; i < threadCnt; i++) {
		userSp = hal_cpuGetUserSP(threadInfo[i].userContext);
		stackSize = coredump_stackSize(userSp, process);
		coredump_dumpPhdr(PT_LOAD, currentOffset, userSp, stackSize, PROT_READ | PROT_WRITE, state);
		currentOffset += stackSize;
	}
#endif

#endif /* COREDUMP_MEM_ALL */
}


static void coredump_dumpAllMemory(process_t *process, coredump_state_t *state)
{
	map_entry_t *e;

	proc_lockSet(&process->mapp->lock);
#ifdef NOMMU
	e = process->entries;
	do {
		if ((e->prot & PROT_READ) && (e->prot & PROT_WRITE)) {
			coredump_encodeChunk(state, e->vaddr, e->size);
		}

		e = e->next;
	} while (e != process->entries);
#else
	e = lib_treeof(map_entry_t, linkage, lib_rbMinimum(process->mapp->tree.root));
	while (e != NULL) {
		if ((e->prot & PROT_READ) && (e->prot & PROT_WRITE)) {
			coredump_encodeChunk(state, e->vaddr, e->size);
		}
		e = lib_treeof(map_entry_t, linkage, lib_rbNext(&e->linkage));
	}
#endif /* NOMMU */
	proc_lockClear(&process->mapp->lock);
}

static size_t coredump_segmentCount(process_t *process)
{
	map_entry_t *e;
	size_t segCnt = 0;

	proc_lockSet(&process->mapp->lock);
#ifdef NOMMU
	e = process->entries;
	do {
		if ((e->prot & PROT_READ) && (e->prot & PROT_WRITE)) {
			segCnt++;
		}

		e = e->next;
	} while (e != process->entries);
#else
	e = lib_treeof(map_entry_t, linkage, lib_rbMinimum(process->mapp->tree.root));
	while (e != NULL) {
		if ((e->prot & PROT_READ) && (e->prot & PROT_WRITE)) {
			segCnt++;
		}
		e = lib_treeof(map_entry_t, linkage, lib_rbNext(&e->linkage));
	}
#endif /* NOMMU */
	proc_lockClear(&process->mapp->lock);
	return segCnt;
}

#endif /* COREDUMP */

void coredump_dump(unsigned int n, exc_context_t *ctx)
{
#ifndef COREDUMP
	return;
#else

	char buff[CORE_BUF_SIZE_MAX];
	coredump_threadinfo_t threadInfo[COREDUMP_THREADS_NUM];
	size_t segCnt;
	size_t threadCnt;
	size_t i;
	coredump_state_t state;
	process_t *process;
	cpu_context_t *cctx = hal_excToCpuCtx(ctx);

	process = proc_current()->process;

	/*
	Ensure for dumped process that:
	- saved context is coherent with stack memory,
	- thread count, section count are fixed,
	while the rest of the processes can run freely.
	*/
	proc_freeze(process);

	threadCnt = coredump_threadsInfo(process, COREDUMP_THREADS_NUM, cctx, threadInfo);
	threadInfo[0].cursig = n;

#ifdef COREDUMP_MEM_ALL
	segCnt = coredump_segmentCount(process);
#elif COREDUMP_MEM_ALL_STACKS
	segCnt = threadCnt;
#else
	segCnt = 1;
#endif

	coredump_init(&state, process->path, hal_exceptionMnemonic(n));
	coredump_dumpElfHeader(segCnt, &state);
	coredump_dumpPhdrs(threadInfo, threadCnt, segCnt, process, cctx, &state);

	coredump_dumpThreadNotes(threadInfo, threadCnt, &state, buff);
	hal_coredumpGeneralAux(buff);
	coredump_encodeChunk(&state, (u8 *)buff, SIZE_COREDUMP_GENAUX);

#ifdef NOMMU
	coredump_dumpLMA(process, &state);
#endif /* NOMMU */

	/* MEMORY */
#ifdef COREDUMP_MEM_ALL
	coredump_dumpAllMemory(process, &state);
#else
	coredump_dumpStack(process, cctx, &state);

#ifdef COREDUMP_MEM_ALL_STACKS
	for (i = 1; i < threadCnt; i++) {
		coredump_dumpStack(process, threadInfo[i].userContext, &state);
	}
#endif

#endif

	coredump_finalize(&state);

	proc_unfreeze(process);
#endif /* COREDUMP */
}
