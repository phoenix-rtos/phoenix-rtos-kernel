/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Processes management
 *
 * Copyright 2012-2015, 2017, 2018 Phoenix Systems
 * Copyright 2001, 2006-2007 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Pawel Kolodziej, Pawel Krezolek, Aleksander Kaminski, Jan Sikorski, Krystian Wasik
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "hal/elf.h"
#include "include/errno.h"
#include "include/signal.h"
#include "vm/vm.h"
#include "lib/lib.h"
#include "posix/posix.h"
#include "process.h"
#include "threads.h"
#include "elf.h"
#include "resource.h"
#include "name.h"
#include "msg.h"
#include "ports.h"
#include "userintr.h"


typedef struct {
	spinlock_t sl;
	thread_t *wq;
	volatile int state;
	thread_t *parent;

	vm_object_t *object;
	off_t offset;
	size_t size;
	vm_map_t *map;
	vm_map_t *imap;
	syspage_prog_t *prog;

	char **argv;
	char **envp;
} process_spawn_t;


static struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
	process_t *first;
	size_t stacksz;
	lock_t lock;
	idtree_t id;
	int idcounter;
} process_common;


process_t *proc_find(int pid)
{
	process_t *p;

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockSet(&process_common.lock);
	p = lib_idtreeof(process_t, idlinkage, lib_idtreeFind(&process_common.id, pid));
	if (p != NULL) {
		p->refs++;
	}
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockClear(&process_common.lock);

	return p;
}


static void process_destroy(process_t *p)
{
	thread_t *ghost;
	vm_map_t *mapp = p->mapp, *imapp = p->imapp;

	perf_kill(p);

	posix_died(process_getPid(p), p->exit);

	/* Destroy resources (especially rtInth) before changing map to prevent race */
	proc_resourcesDestroy(p);

	proc_changeMap(p, NULL, NULL, NULL);

	if (mapp != NULL) {
		vm_mapDestroy(p, mapp);
	}

	if (imapp != NULL) {
		vm_mapDestroy(p, imapp);
	}

	proc_portsDestroy(p);
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockDone(&p->lock);

	while ((ghost = p->ghosts) != NULL) {
		LIST_REMOVE_EX(&p->ghosts, ghost, procnext, procprev);
		vm_kfree(ghost);
	}

	vm_kfree(p->path);
	vm_kfree(p->argv);
	vm_kfree(p->envp);
	vm_kfree(p);
}


int proc_put(process_t *p)
{
	int remaining;

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockSet(&process_common.lock);
	remaining = --p->refs;
	LIB_ASSERT(remaining >= 0, "pid: %d, refcnt became negative", process_getPid(p));
	if (remaining <= 0) {
		lib_idtreeRemove(&process_common.id, &p->idlinkage);
	}
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockClear(&process_common.lock);

	if (remaining <= 0) {
		process_destroy(p);
	}

	return remaining;
}


void proc_get(process_t *p)
{
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockSet(&process_common.lock);
	LIB_ASSERT(p->refs > 0, "pid: %d, got reference on process with zero references",
			process_getPid(p));
	++p->refs;
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockClear(&process_common.lock);
}


static int process_alloc(process_t *process)
{
	int id;

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockSet(&process_common.lock);
	id = lib_idtreeAlloc(&process_common.id, &process->idlinkage, process_common.idcounter);
	if (id < 0) {
		/* Try from the start */
		process_common.idcounter = 1;
		id = lib_idtreeAlloc(&process_common.id, &process->idlinkage, process_common.idcounter);
	}

	if (id >= 0) {
		if (process_common.idcounter == MAX_PID) {
			// lewą stronę zrzutować na unsigned long long czy prawą na int?
			process_common.idcounter = 1;
		}
		else {
			process_common.idcounter++;
		}
	}
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockClear(&process_common.lock);

	return id;
}


int proc_start(void (*initthr)(void *), void *arg, const char *path)
{
	process_t *process;
	process = vm_kmalloc(sizeof(process_t));
	if (process == NULL) {
		return -ENOMEM;
	}

#ifdef NOMMU
	process->entries = NULL;
#endif

	process->path = NULL;

	if (path != NULL) {
		process->path = lib_strdup(path);
		if (process->path == NULL) {
			vm_kfree(process);
			return -ENOMEM;
		}
	}

	process->argv = NULL;
	process->envp = NULL;
	process->threads = NULL;
	process->ghosts = NULL;
	process->reaper = NULL;
	process->refs = 1;

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockInit(&process->lock, &proc_lockAttrDefault, "process");

	process->ports = NULL;

	process->sigpend = 0;
	process->sigmask = 0;
	process->sighandler = NULL;
	/* MISRA Rule 11.6: ptr_t is typedef of unsigned int, NULL is not pointer to void*/
	process->tls.tls_base = 0;
	process->tls.tbss_sz = 0;
	process->tls.tdata_sz = 0;
	process->tls.tls_sz = 0;
	/* MISRA Rule 11.6: ptr_t is typedef of unsigned int, NULL is not pointer to void*/
	process->tls.arm_m_tls = 0;

#ifndef NOMMU
	process->lazy = 0;
#else
	process->lazy = 1;
#endif

	proc_changeMap(process, NULL, NULL, NULL);

	/* Initialize resources tree for mutex and cond handles */
	_resource_init(process);
	/* MISRAC2012-RULE_17_7-a */
	(void)process_alloc(process);
	perf_fork(process);

	if (proc_threadCreate(process, initthr, NULL, 4, SIZE_KSTACK, NULL, 0, (void *)arg) < 0) {
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_put(process);
		return -EINVAL;
	}

	return process_getPid(process);
}


void proc_kill(process_t *proc)
{
	proc_threadsDestroy(&proc->threads);
}


void process_dumpException(unsigned int n, exc_context_t *ctx)
{
	thread_t *thread;
	process_t *process;
	userintr_t *intr;
	char buff[SIZE_CTXDUMP];
	int len;

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

	/* MISRAC2012-RULE_17_7-a */
	(void)posix_write(2, buff, hal_strlen(buff));
	(void)posix_write(2, "\n", 1);

	/* use proc_current() as late as possible - to be able to print exceptions in scheduler */
	thread = proc_current();
	process = thread->process;

	intr = userintr_active();

	if (intr != NULL) {
		len = lib_sprintf(buff, "in interrupt (%u) handler of process \"%s\" (PID: %u)\n", intr->handler.n, intr->process->path, process_getPid(intr->process));
	}
	else if (process == NULL) {
		len = lib_sprintf(buff, "in kernel thread %lu\n", proc_getTid(thread));
	}
	else {
		len = lib_sprintf(buff, "in thread %lu, process \"%s\" (PID: %u)\n", proc_getTid(thread), process->path, process_getPid(process));
	}

	/* MISRAC2012-RULE_17_7-a */
	(void)posix_write(2, buff, (size_t)len);
}


static void process_exception(unsigned int n, exc_context_t *ctx)
{
	thread_t *thread = proc_current();

	process_dumpException(n, ctx);

	if (thread->process == NULL) {
		hal_cpuHalt();
	}

	/* MISRAC2012-RULE_17_7-a */
	(void)threads_sigpost(thread->process, thread, signal_kill);

	/* Don't allow current thread to return to the userspace,
	 * it will crash anyway. */
	thread->exit = THREAD_END_NOW;

	/* MISRAC2012-RULE_17_7-a */
	(void)hal_cpuReschedule(NULL, NULL);
}


static void process_illegal(unsigned int n, exc_context_t *ctx)
{
	thread_t *thread = proc_current();
	process_t *process = thread->process;

	if (process == NULL) {
		hal_cpuHalt();
	}

	/* MISRAC2012-RULE_17_7-a */
	(void)threads_sigpost(process, thread, signal_illegal);
}


static void process_tlsAssign(hal_tls_t *process_tls, hal_tls_t *tls, ptr_t tbssAddr)
{
	/* MISRA Rule 11.6: NULL can not be longe used as we changed it to voit pointer!!! */
	if (tls->tls_base != 0U) {
		process_tls->tls_base = tls->tls_base;
	}
	else if (tbssAddr != 0U) {
		process_tls->tls_base = tbssAddr;
	}
	else {
		process_tls->tls_base = 0;
	}
	process_tls->tdata_sz = tls->tdata_sz;
	process_tls->tbss_sz = tls->tbss_sz;
	process_tls->tls_sz = (tls->tbss_sz + tls->tdata_sz + sizeof(void *) + sizeof(void *) - 1U) & ~(sizeof(void *) - 1U);
	process_tls->arm_m_tls = tls->arm_m_tls;
}


static int process_isPtrValid(void *mapStart, size_t mapSize, void *ptrStart, size_t ptrSize)
{
	void *mapEnd = ((char *)mapStart) + mapSize;
	void *ptrEnd = ((char *)ptrStart) + ptrSize;

	/* clang-format off */
	return ((ptrSize == 0U) ||
		((ptrEnd > mapStart) && (ptrEnd <= mapEnd) &&
		(ptrStart >= mapStart) && (ptrStart < ptrEnd))) ?
		1 : 0;
	/* clang-format on */
}


/* Ensure kernel won't make a bad access on a malformed elf during load. */
static int process_validateElf32(void *iehdr, size_t size)
{
	Elf32_Ehdr *ehdr = iehdr;
	Elf32_Phdr *phdr;
	Elf32_Shdr *shdr, *shstrshdr;
	char *snameTab;
	size_t memsz, filesz;
	unsigned i, misalign;
	off_t offs;

	if (size < sizeof(*ehdr)) {
		return -ENOEXEC;
	}

	/* Validate header. */
	if (((process_isPtrValid(iehdr, size, ehdr->e_ident, 4) == 0) ||
				(hal_strncmp((char *)ehdr->e_ident, "\177ELF", 4) != 0)) ||
			(ehdr->e_shnum == 0U)) {
		return -ENOEXEC;
	}

	phdr = (void *)ehdr + ehdr->e_phoff;
	if (process_isPtrValid(iehdr, size, phdr, sizeof(*phdr) * ehdr->e_phnum) == 0) {
		return -ENOEXEC;
	}
	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdr->p_type != PT_LOAD) {
			if (process_isPtrValid(iehdr, size, ((char *)ehdr) + phdr[i].p_offset, phdr[i].p_filesz) == 0) {
				return -ENOEXEC;
			}
			continue;
		}

		offs = (off_t)((Elf32_Off)(phdr->p_offset & ~(phdr->p_align - 1U)));
		misalign = phdr->p_offset & (phdr->p_align - 1U);
		filesz = (phdr->p_filesz != 0U) ? (phdr->p_filesz + misalign) : 0;
		memsz = phdr->p_memsz + misalign;
		if ((offs >= (off_t)size) || (memsz < filesz)) {
			return -ENOEXEC;
		}
	}

	shdr = (void *)((char *)ehdr + ehdr->e_shoff);
	if (process_isPtrValid(iehdr, size, shdr, sizeof(*shdr) * ehdr->e_shnum) == 0) {
		return -ENOEXEC;
	}

	shstrshdr = shdr + ehdr->e_shstrndx;
	if (process_isPtrValid(iehdr, size, shstrshdr, sizeof(*shstrshdr)) == 0) {
		return -ENOEXEC;
	}
	snameTab = (char *)ehdr + shstrshdr->sh_offset;
	if (process_isPtrValid(iehdr, size, snameTab, shstrshdr->sh_size) == 0) {
		return -ENOEXEC;
	}
	/* Strings must end with NULL character. */
	if ((shstrshdr->sh_size != 0U) && (snameTab[shstrshdr->sh_size - 1U] != '\0')) {
		return -ENOEXEC;
	}

	for (i = 0; i < ehdr->e_shnum; i++) {
		if (((shdr[i].sh_type != SHT_NOBITS) &&
					(process_isPtrValid(iehdr, size, ((char *)ehdr) + shdr[i].sh_offset, shdr[i].sh_size) == 0)) ||
				(shdr->sh_name >= shstrshdr->sh_size)) {
			return -ENOEXEC;
		}
	}

	return 0;
}


#ifndef NOMMU


static int process_validateElf64(void *iehdr, size_t size)
{
	Elf64_Ehdr *ehdr = iehdr;
	Elf64_Phdr *phdr;
	Elf64_Shdr *shdr, *shstrshdr;
	char *snameTab;
	size_t memsz, filesz;
	unsigned i, misalign;
	off_t offs;

	if (size < sizeof(*ehdr)) {
		return -ENOEXEC;
	}

	/* Validate header. */
	if (((process_isPtrValid(iehdr, size, ehdr->e_ident, 4) == 0) ||
				(hal_strncmp((char *)ehdr->e_ident, "\177ELF", 4) != 0)) ||
			(ehdr->e_shnum == 0)) {
		return -ENOEXEC;
	}

	phdr = (void *)ehdr + ehdr->e_phoff;
	if (process_isPtrValid(iehdr, size, phdr, sizeof(*phdr) * ehdr->e_phnum) == 0) {
		return -ENOEXEC;
	}
	for (i = 0; i < ehdr->e_phnum; i++) {
		if (phdr->p_type != PT_LOAD) {
			if (process_isPtrValid(iehdr, size, ((char *)ehdr) + phdr[i].p_offset, phdr[i].p_filesz) == 0) {
				return -ENOEXEC;
			}
			continue;
		}

		offs = phdr->p_offset & ~(phdr->p_align - 1);
		misalign = phdr->p_offset & (phdr->p_align - 1);
		filesz = phdr->p_filesz ? (phdr->p_filesz + misalign) : 0;
		memsz = phdr->p_memsz + misalign;
		if ((offs >= size) || (memsz < filesz)) {
			return -ENOEXEC;
		}
	}

	shdr = (void *)((char *)ehdr + ehdr->e_shoff);
	if (process_isPtrValid(iehdr, size, shdr, sizeof(*shdr) * ehdr->e_shnum) == 0) {
		return -ENOEXEC;
	}

	shstrshdr = shdr + ehdr->e_shstrndx;
	if (process_isPtrValid(iehdr, size, shstrshdr, sizeof(*shstrshdr)) == 0) {
		return -ENOEXEC;
	}
	snameTab = (char *)ehdr + shstrshdr->sh_offset;
	if (process_isPtrValid(iehdr, size, snameTab, shstrshdr->sh_size) == 0) {
		return -ENOEXEC;
	}
	/* Strings must end with NULL character. */
	if (snameTab[shstrshdr->sh_size - 1] != '\0') {
		return -ENOEXEC;
	}

	for (i = 0; i < ehdr->e_shnum; i++) {
		if (((shdr[i].sh_type != SHT_NOBITS) &&
					process_isPtrValid(iehdr, size, ((char *)ehdr) + shdr[i].sh_offset, shdr[i].sh_size) == 0) ||
				(shdr->sh_name >= shstrshdr->sh_size)) {
			return -ENOEXEC;
		}
	}

	return 0;
}


/* TODO - adding error handling and unmapping of already mapped segments */
int process_load32(vm_map_t *map, vm_object_t *o, off_t base, void *iehdr, size_t size, size_t *ustacksz, hal_tls_t *tls, ptr_t *tbssAddr)
{
	void *vaddr;
	size_t memsz, filesz;
	Elf32_Ehdr *ehdr = iehdr;
	Elf32_Phdr *phdr;
	Elf32_Shdr *shdr, *shstrshdr;
	unsigned i, prot, flags, misalign;
	off_t offs;
	char *snameTab;

	if (process_validateElf32(iehdr, size) < 0) {
		return -ENOEXEC;
	}

	shdr = (void *)((char *)ehdr + ehdr->e_shoff);
	shstrshdr = shdr + ehdr->e_shstrndx;
	snameTab = (char *)ehdr + shstrshdr->sh_offset;
	/* Find .tdata and .tbss sections */
	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		if (hal_strcmp(&snameTab[shdr->sh_name], ".tdata") == 0) {
			tls->tls_base = (ptr_t)shdr->sh_addr;
			tls->tdata_sz += shdr->sh_size;
		}
		else if (hal_strcmp(&snameTab[shdr->sh_name], ".tbss") == 0) {
			*tbssAddr = (ptr_t)shdr->sh_addr;
			tls->tbss_sz += shdr->sh_size;
		}
		else if (hal_strcmp(&snameTab[shdr->sh_name], "armtls") == 0) {
			tls->arm_m_tls = (ptr_t)shdr->sh_addr;
		}
	}

	for (i = 0, phdr = (void *)ehdr + ehdr->e_phoff; i < ehdr->e_phnum; i++, phdr++) {
		if ((phdr->p_type == PT_GNU_STACK) && (phdr->p_memsz != 0)) {
			*ustacksz = round_page(phdr->p_memsz);
		}

		if ((phdr->p_type != PT_LOAD) || (phdr->p_vaddr == 0)) {
			continue;
		}

		vaddr = (void *)((ptr_t)(phdr->p_vaddr & ~(phdr->p_align - 1)));
		offs = phdr->p_offset & ~(phdr->p_align - 1);
		misalign = phdr->p_offset & (phdr->p_align - 1);
		filesz = phdr->p_filesz ? (phdr->p_filesz + misalign) : 0;
		memsz = phdr->p_memsz + misalign;

		prot = PROT_USER;
		flags = MAP_NONE;

		if ((phdr->p_flags & PF_R) != 0) {
			prot |= PROT_READ;
		}

		if ((phdr->p_flags & PF_W) != 0) {
			prot |= PROT_WRITE;
		}

		if ((phdr->p_flags & PF_X) != 0) {
			prot |= PROT_EXEC;
		}

		if ((filesz != 0) && ((prot & PROT_WRITE) != 0)) {
			flags |= MAP_NEEDSCOPY;
		}

		if ((filesz != 0) && (vm_mmap(map, vaddr, NULL, round_page(filesz), prot, o, base + offs, flags) == NULL)) {
			return -ENOMEM;
		}

		if (filesz != memsz) {
			if ((round_page(memsz) != round_page(filesz)) && (vm_mmap(map, vaddr, NULL, round_page(memsz) - round_page(filesz), prot, NULL, -1, MAP_NONE) == NULL)) {
				return -ENOMEM;
			}

			hal_memset(vaddr + filesz, 0, round_page((ptr_t)vaddr + memsz) - ((ptr_t)vaddr + filesz));
		}
	}
	return EOK;
}


int process_load64(vm_map_t *map, vm_object_t *o, off_t base, void *iehdr, size_t size, size_t *ustacksz, hal_tls_t *tls, ptr_t *tbssAddr)
{
	void *vaddr;
	size_t memsz, filesz;
	Elf64_Ehdr *ehdr = iehdr;
	Elf64_Phdr *phdr;
	Elf64_Shdr *shdr, *shstrshdr;
	unsigned i, prot, flags, misalign;
	off_t offs;
	char *snameTab;

	if (process_validateElf64(iehdr, size) < 0) {
		return -ENOEXEC;
	}

	shdr = (void *)((char *)ehdr + ehdr->e_shoff);
	shstrshdr = shdr + ehdr->e_shstrndx;
	snameTab = (char *)ehdr + shstrshdr->sh_offset;
	/* Find .tdata and .tbss sections */
	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		if (hal_strcmp(&snameTab[shdr->sh_name], ".tdata") == 0) {
			tls->tls_base = (ptr_t)shdr->sh_addr;
			tls->tdata_sz += shdr->sh_size;
		}
		else if (hal_strcmp(&snameTab[shdr->sh_name], ".tbss") == 0) {
			*tbssAddr = (ptr_t)shdr->sh_addr;
			tls->tbss_sz += shdr->sh_size;
		}
		else if (hal_strcmp(&snameTab[shdr->sh_name], "armtls") == 0) {
			tls->arm_m_tls = (ptr_t)shdr->sh_addr;
		}
	}

	for (i = 0, phdr = (void *)ehdr + ehdr->e_phoff; i < ehdr->e_phnum; i++, phdr++) {
		if ((phdr->p_type == PT_GNU_STACK) && (phdr->p_memsz != 0)) {
			*ustacksz = round_page(phdr->p_memsz);
		}

		if ((phdr->p_type != PT_LOAD) || (phdr->p_vaddr == 0)) {
			continue;
		}

		vaddr = (void *)((ptr_t)(phdr->p_vaddr & ~((ptr_t)phdr->p_align - 1)));
		offs = phdr->p_offset & ~(phdr->p_align - 1);
		misalign = phdr->p_offset & (phdr->p_align - 1);
		filesz = phdr->p_filesz ? (phdr->p_filesz + misalign) : 0;
		memsz = phdr->p_memsz + misalign;

		prot = PROT_USER;
		flags = MAP_NONE;

		if ((phdr->p_flags & PF_R) != 0) {
			prot |= PROT_READ;
		}

		if ((phdr->p_flags & PF_W) != 0) {
			prot |= PROT_WRITE;
		}

		if ((phdr->p_flags & PF_X) != 0) {
			prot |= PROT_EXEC;
		}

		if ((filesz != 0) && ((prot & PROT_WRITE) != 0)) {
			flags |= MAP_NEEDSCOPY;
		}

		if ((filesz != 0) && (vm_mmap(map, vaddr, NULL, round_page(filesz), prot, o, base + offs, flags) == NULL)) {
			return -ENOMEM;
		}

		if (filesz != memsz) {
			if ((round_page(memsz) != round_page(filesz)) && (vm_mmap(map, vaddr, NULL, round_page(memsz) - round_page(filesz), prot, NULL, -1, MAP_NONE) == NULL)) {
				return -ENOMEM;
			}

			hal_memset(vaddr + filesz, 0, round_page((ptr_t)vaddr + memsz) - ((ptr_t)vaddr + filesz));
		}
	}
	return EOK;
}


int process_load(process_t *process, vm_object_t *o, off_t base, size_t size, void **ustack, void **entry)
{
	void *stack;
	Elf64_Ehdr *ehdr;
	vm_map_t *map = process->mapp;
	size_t ustacksz = SIZE_USTACK;
	int err = EOK;
	hal_tls_t tlsNew;
	ptr_t tbssAddr = 0;

	tlsNew.tls_base = NULL;
	tlsNew.tdata_sz = 0;
	tlsNew.tbss_sz = 0;
	tlsNew.tls_sz = 0;
	tlsNew.arm_m_tls = NULL;

	size = round_page(size);

	ehdr = vm_mmap(process_common.kmap, NULL, NULL, size, PROT_READ, o, base, MAP_NONE);
	if (ehdr == NULL) {
		return -ENOMEM;
	}

	switch (ehdr->e_ident[4]) {
		/* 32-bit binary */
		case 1:
			*entry = (void *)(ptr_t)((Elf32_Ehdr *)ehdr)->e_entry;
			err = process_load32(map, o, base, ehdr, size, &ustacksz, &tlsNew, &tbssAddr);
			break;

		/* 64-bit binary */
		case 2:
			*entry = (void *)(ptr_t)ehdr->e_entry;
			err = process_load64(map, o, base, ehdr, size, &ustacksz, &tlsNew, &tbssAddr);
			break;

		default:
			err = -ENOEXEC;
	}
	vm_munmap(process_common.kmap, ehdr, size);

	if (err < 0) {
		return err;
	}

	process_tlsAssign(&process->tls, &tlsNew, tbssAddr);

	/* Allocate and map user stack */
	stack = vm_mmap(map, map->pmap.end - ustacksz, NULL, ustacksz, PROT_READ | PROT_WRITE | PROT_USER, NULL, -1, MAP_NONE);
	if (stack == NULL) {
		return -ENOMEM;
	}

	*ustack = stack + ustacksz;

	threads_canaryInit(proc_current(), stack);

	return EOK;
}

#else

struct _reloc {
	void *vbase;
	void *pbase;
	size_t size;
	unsigned int misalign;
};


static int process_relocate(struct _reloc *reloc, size_t relocsz, char **addr)
{
	size_t i;

	if ((ptr_t)(*addr) == 0U) {
		return 0;
	}

	for (i = 0; i < relocsz; ++i) {
		/* MISRA Rule 11.6: (unsigned int *) added in line 735 x 2 and in line x 3 */
		if ((ptr_t)(unsigned int *)reloc[i].vbase <= (ptr_t)(*addr) && (ptr_t)(unsigned int *)reloc[i].vbase + reloc[i].size > (ptr_t)(*addr)) {
			(*addr) = (void *)(unsigned int *)((ptr_t)(*addr) - (ptr_t)(unsigned int *)reloc[i].vbase + (ptr_t)(unsigned int *)reloc[i].pbase);
			return 0;
		}
	}

	return -1;
}


int process_load(process_t *process, vm_object_t *o, off_t base, size_t size, void **ustack, void **entry)
// TBD_Julia jak zakomentuje się else to problem znika, ta sama funkcja wyżej w ifndef nie pokazuje tego błedu
{
	void *stack, *paddr;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	Elf32_Shdr *shdr, *shstrshdr;
	Elf32_Rela rela;
	unsigned relocsz = 0, prot, flags, reloffs;
	int badreloc = 0, error;
	unsigned i, j;
	void *relptr;
	char *snameTab;
	ptr_t *got;
	struct _reloc reloc[8];
	size_t stacksz = SIZE_USTACK;
	hal_tls_t tlsNew;
	ptr_t tbssAddr = 0;

	if (o != VM_OBJ_PHYSMEM) {
		return -ENOEXEC;
	}

	ehdr = (void *)(unsigned int *)(ptr_t)base;

	error = process_validateElf32(ehdr, size);
	if (error < 0) {
		return error;
	}

	hal_memset(reloc, 0, sizeof(reloc));

	for (i = 0U, j = 0U, phdr = (void *)ehdr + ehdr->e_phoff; i < ehdr->e_phnum; i++, phdr++) {
		if (phdr->p_type == PT_GNU_STACK && phdr->p_memsz != 0U) {
			stacksz = round_page(phdr->p_memsz);
		}

		if (phdr->p_type != PT_LOAD) {
			continue;
		}

		reloffs = 0;
		prot = PROT_USER;
		flags = MAP_NONE;
		paddr = (char *)ehdr + phdr->p_offset;

		if ((phdr->p_flags & PF_R) != 0U) {
			prot |= PROT_READ;
		}

		if ((phdr->p_flags & PF_X) != 0U) {
			prot |= PROT_EXEC;

			if ((process->imapp != NULL) &&
					/* MISRA Rule 11.6: Added (unsigned int *) after (ptr_t) in line 799, 800 and 809*/
					(((ptr_t)base < (ptr_t)(unsigned int *)process->imapp->start) ||
							((ptr_t)base > (ptr_t)(unsigned int *)process->imapp->stop))) {
				paddr = vm_mmap(process->imapp, NULL, NULL, round_page(phdr->p_memsz), (u8)prot, NULL, -1, (u8)flags);
				if (paddr == NULL) {
					return -ENOMEM;
				}

				hal_memcpy((char *)paddr, (char *)ehdr + phdr->p_offset, phdr->p_filesz);

				/* Need to make cache and memorSy coherent, so $I is coherent too */
				hal_cleanDCache((ptr_t)(unsigned int *)paddr, phdr->p_memsz);
			}
		}

		if ((phdr->p_flags & PF_W) != 0U) {
			prot |= PROT_WRITE;

			reloffs = phdr->p_vaddr % SIZE_PAGE;

			paddr = vm_mmap(process->mapp, NULL, NULL, round_page(phdr->p_memsz + reloffs), (u8)prot, NULL, -1, (u8)flags);
			if (paddr == NULL) {
				return -ENOMEM;
			}

			if (phdr->p_filesz != 0U) {
				if ((phdr->p_offset + phdr->p_filesz) > size) {
					return -ENOEXEC;
				}

				hal_memcpy((char *)paddr + reloffs, (char *)ehdr + phdr->p_offset, phdr->p_filesz);
			}

			hal_memset((char *)paddr, 0, reloffs);
			hal_memset((char *)paddr + reloffs + phdr->p_filesz, 0, round_page(phdr->p_memsz + reloffs) - phdr->p_filesz - reloffs);
		}

		if (j >= (sizeof(reloc) / sizeof(reloc[0]))) {
			return -ENOMEM;
		}

		/* MISRA Rule 11.6: Added (unsinged int *)*/
		reloc[j].vbase = (void *)(unsigned int *)phdr->p_vaddr;
		reloc[j].pbase = (void *)((char *)paddr + reloffs);
		reloc[j].size = phdr->p_memsz;
		reloc[j].misalign = phdr->p_offset & (phdr->p_align - 1U);
		++relocsz;
		++j;
	}

	shdr = (void *)((char *)ehdr + ehdr->e_shoff);
	shstrshdr = shdr + ehdr->e_shstrndx;

	snameTab = (char *)ehdr + shstrshdr->sh_offset;

	/* Find .got section */
	for (i = 0U; i < ehdr->e_shnum; i++, shdr++) {
		if (hal_strcmp(&snameTab[shdr->sh_name], ".got") == 0) {
			break;
		}
	}

	if (i >= ehdr->e_shnum) {
		return -ENOEXEC;
	}

	got = (ptr_t *)shdr->sh_addr;
	if (process_relocate(reloc, relocsz, (char **)&got) < 0) {
		return -ENOEXEC;
	}

	/* Perform .got relocations */
	/* This is non classic approach to .got relocation. We use .got itselft
	 * instead of .rel section. */
	for (i = 0U; i < shdr->sh_size / 4U; ++i) {
		if (process_relocate(reloc, relocsz, (char **)&got[i]) < 0) {
			return -ENOEXEC;
		}
	}

	/* MISRA Rule 11.6: Added (unsinged long *)*/
	*entry = (void *)(unsigned long *)(unsigned long)ehdr->e_entry;
	if (process_relocate(reloc, relocsz, (char **)entry) < 0) {
		return -ENOEXEC;
	}

	/* Perform data, init_array and fini_array relocation */
	for (i = 0U, shdr = (void *)((char *)ehdr + ehdr->e_shoff); i < ehdr->e_shnum; i++, shdr++) {
		if ((shdr->sh_size == 0U) || (shdr->sh_entsize == 0U)) {
			continue;
		}

		/* strncmp as there may be multiple .rela.* or .rel.* sections for different sections. */
		if ((hal_strncmp(&snameTab[shdr->sh_name], ".rela.", 6) != 0) && (hal_strncmp(&snameTab[shdr->sh_name], ".rel.", 5) != 0)) {
			continue;
		}

		for (j = 0U; j < (shdr->sh_size / shdr->sh_entsize); ++j) {
			/* Valid for both Elf32_Rela and Elf32_Rel, due to correct size being stored in shdr->sh_entsize. */
			/* For .rel. section make sure not to access addend field! */
			hal_memcpy(&rela, (Elf32_Rela *)((ptr_t)shdr->sh_offset + (ptr_t)ehdr + (j * shdr->sh_entsize)), shdr->sh_entsize);

			if (hal_isRelReloc(ELF32_R_TYPE(rela.r_info)) == 0) {
				// TBD_Bart Thr only thing that macro is doing is casting.
				continue;
			}

			/* MISRA Rule 11.6: Added (unsinged int *)*/
			relptr = (void *)(unsigned int *)rela.r_offset;
			if (process_relocate(reloc, relocsz, (char **)&relptr) < 0) {
				return -ENOEXEC;
			}

			/* Don't modify ELF file! */
			/* MISRA Rule 11.6: Added (unsinged int *) x2 */
			if (((ptr_t)(unsigned int *)relptr >= (ptr_t)base) && ((ptr_t)(unsigned int *)relptr < ((ptr_t)base + size))) {
				++badreloc;
				continue;
			}

			/* NOTE: Build process on NOMMU compiles a position-dependend binary but kernel treats it as a PIE. */
			/* There is no need to look at the symbol and perform calculations as it is already done by static linker. */

			if (process_relocate(reloc, relocsz, relptr) < 0) {
				return -ENOEXEC;
			}
		}
	}

	/* MISRA Rule 11.6: NULL is now a voind ptrs and we can not use it here!!!*/
	tlsNew.tls_base = 0;
	tlsNew.tdata_sz = 0;
	tlsNew.tbss_sz = 0;
	tlsNew.tls_sz = 0;
	tlsNew.arm_m_tls = 0;

	/* Perform .tdata, .tbss and .armtls relocations */
	for (i = 0U, shdr = (void *)((char *)ehdr + ehdr->e_shoff); i < ehdr->e_shnum; i++, shdr++) {
		if (hal_strcmp(&snameTab[shdr->sh_name], ".tdata") == 0) {
			tlsNew.tls_base = (ptr_t)shdr->sh_addr;
			tlsNew.tdata_sz += shdr->sh_size;
			if (process_relocate(reloc, relocsz, (char **)&tlsNew.tls_base) < 0) {
				return -ENOEXEC;
			}
		}
		else if (hal_strcmp(&snameTab[shdr->sh_name], ".tbss") == 0) {
			tbssAddr = (ptr_t)shdr->sh_addr;
			tlsNew.tbss_sz += shdr->sh_size;
			if (process_relocate(reloc, relocsz, (char **)&tbssAddr) < 0) {
				return -ENOEXEC;
			}
		}
		else if (hal_strcmp(&snameTab[shdr->sh_name], "armtls") == 0) {
			tlsNew.arm_m_tls = (ptr_t)shdr->sh_addr;
			if (process_relocate(reloc, relocsz, (char **)&tlsNew.arm_m_tls) < 0) {
				return -ENOEXEC;
			}
		}
	}
	process_tlsAssign(&process->tls, &tlsNew, tbssAddr);

	/* Allocate and map user stack */
	stack = vm_mmap(process->mapp, NULL, NULL, stacksz, PROT_READ | PROT_WRITE | PROT_USER, NULL, -1, MAP_NONE);
	if (stack == NULL) {
		return -ENOMEM;
	}

	process->got = (void *)got;
	*ustack = stack + stacksz;

	threads_canaryInit(proc_current(), stack);

	if (badreloc != 0) {
		if ((process->path != NULL) && (process->path[0] != '\0')) {
			/* MISRAC2012-RULE_17_7-a */
			(void)lib_printf("app %s: ", process->path);
		}
		else {
			/* MISRAC2012-RULE_17_7-a */
			(void)lib_printf("process %d: ", process_getPid(process));
		}

		/* MISRAC2012-RULE_17_7-a */
		(void)lib_printf("Found %d badreloc%c\n", badreloc, (badreloc > 1) ? 's' : ' ');
	}

	return EOK;
}

#endif

static void *proc_copyargs(char **args)
{
	unsigned int argc, len = 0U;
	void *storage;
	char **kargs, *p;

	if (args == NULL) {
		return NULL;
	}

	for (argc = 0U; args[argc] != NULL; ++argc) {
		len += hal_strlen(args[argc]) + 1U;
	}

	len += (argc + 1U) * sizeof(char *);

	if ((kargs = storage = vm_kmalloc(len)) == NULL) {
		return NULL;
	}

	kargs[argc] = NULL;

	p = (char *)storage + (argc + 1U) * sizeof(char *);

	while (argc-- > 0U) {
		len = hal_strlen(args[argc]) + 1U;
		hal_memcpy(p, args[argc], len);
		kargs[argc] = p;
		p += len;
	}

	return storage;
}


static void *process_putargs(void *stack, char ***argsp, int *count)
{
	unsigned int argc;
	size_t len;
	char **args_stack, **args = *argsp;

	for (argc = 0; (args != NULL) && (args[argc] != NULL); ++argc) {
	}

	stack -= (argc + 1U) * sizeof(char *);
	args_stack = stack;
	args_stack[argc] = NULL;

	for (argc = 0; (args != NULL) && (args[argc] != NULL); ++argc) {
		len = hal_strlen(args[argc]) + 1U;
		stack -= SIZE_STACK_ARG(len);
		hal_memcpy(stack, args[argc], len);
		args_stack[argc] = stack;
	}

	*argsp = args_stack;
	*count = (int)argc;

	return stack;
}


static void process_exec(thread_t *current, process_spawn_t *spawn)
{
	void *stack, *entry;
	int err = 0, count;
	void *cleanupFn = NULL;
	unsigned int i;
	spinlock_ctx_t sc;
	const struct stackArg args[] = {
		{ &spawn->envp, sizeof(spawn->envp) },
		{ &spawn->argv, sizeof(spawn->argv) },
		{ &count, sizeof(count) },
		{ &cleanupFn, sizeof(cleanupFn) }
	};

	current->process->argv = spawn->argv;
	current->process->envp = spawn->envp;

#ifndef NOMMU
	vm_mapCreate(&current->process->map, (void *)(VADDR_MIN + SIZE_PAGE), (void *)VADDR_USR_MAX);
	proc_changeMap(current->process, &current->process->map, NULL, &current->process->map.pmap);
	(void)i;
#else
	/* MISRAC2012-RULE_17_7-a */
	(void)pmap_create(&current->process->map.pmap, NULL, NULL, NULL);
	proc_changeMap(current->process, (spawn->map != NULL) ? spawn->map : process_common.kmap, spawn->imap, &current->process->map.pmap);
	current->process->entries = NULL;

	if (spawn->prog != NULL) {
		/* Add instruction maps */
		for (i = 0; (i < spawn->prog->imapSz) && (err == 0); ++i) {
			err = pmap_addMap(current->process->pmapp, spawn->prog->imaps[i]);
		}

		/* Add data/io maps */
		for (i = 0; (i < spawn->prog->dmapSz) && (err == 0); ++i) {
			err = pmap_addMap(current->process->pmapp, spawn->prog->dmaps[i]);
		}
	}
#endif

	pmap_switch(current->process->pmapp);

	if (err == 0) {
		err = process_load(current->process, spawn->object, spawn->offset, spawn->size, &stack, &entry);
	}

	if (err == 0) {
		stack = process_putargs(stack, &spawn->envp, &count);
		stack = process_putargs(stack, &spawn->argv, &count);
		hal_stackPutArgs(&stack, sizeof(args) / sizeof(args[0]), args);
	}

	if (spawn->parent == NULL) {
		/* if execing without vfork */
		hal_spinlockDestroy(&spawn->sl);
		/* MISRAC2012-RULE_17_7-a */
		(void)vm_objectPut(spawn->object);
	}
	else {
		hal_spinlockSet(&spawn->sl, &sc);
		spawn->state = FORKED;
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_threadWakeup(&spawn->wq);
		hal_spinlockClear(&spawn->sl, &sc);
	}

	/* MISRA Rule 11.6: NULL is now a voind pointer!!!*/
	if ((err == EOK) && (current->process->tls.tls_base != 0U)) {
		err = process_tlsInit(&current->tls, &current->process->tls, current->process->mapp);
	}

	if (err != 0) {
		current->process->exit = err;
		proc_threadEnd();
	}

	hal_cpuDisableInterrupts();
	_hal_cpuSetKernelStack(current->kstack + current->kstacksz);
	hal_cpuSetGot(current->process->got);

	if (current->tls.tls_base != 0U) {
		hal_cpuTlsSet(&current->tls, current->context);
	}

	hal_cpuSmpSync();
	hal_jmp(entry, current->kstack + current->kstacksz, stack, 0, NULL);
}


static void proc_spawnThread(void *arg)
{
	thread_t *current = proc_current();
	process_spawn_t *spawn = arg;

	/* temporary: create new posix process */
	if (spawn->parent != NULL) {
		/* MISRAC2012-RULE_17_7-a */
		(void)posix_clone(process_getPid(spawn->parent->process));
	}

	process_exec(current, spawn);
}


static int proc_spawn(vm_object_t *object, syspage_prog_t *prog, vm_map_t *imap, vm_map_t *map, off_t offset, size_t size, const char *path, char **argv, char **envp)
{
	int pid;
	process_spawn_t spawn;
	spinlock_ctx_t sc;

	if (argv != NULL && (argv = proc_copyargs(argv)) == NULL) {
		return -ENOMEM;
	}

	if (envp != NULL && (envp = proc_copyargs(envp)) == NULL) {
		vm_kfree(argv);
		return -ENOMEM;
	}

	spawn.object = object;
	spawn.offset = offset;
	spawn.size = size;
	spawn.wq = NULL;
	spawn.state = FORKING;
	spawn.argv = argv;
	spawn.envp = envp;
	spawn.parent = proc_current();
	spawn.map = map;
	spawn.imap = imap;
	spawn.prog = prog;

	hal_spinlockCreate(&spawn.sl, "spawnsl");

	if ((pid = proc_start(proc_spawnThread, &spawn, path)) > 0) {
		hal_spinlockSet(&spawn.sl, &sc);
		while (spawn.state == FORKING) {
			/* MISRAC2012-RULE_17_7-a */
			(void)proc_threadWait(&spawn.wq, &spawn.sl, 0, &sc);
		}
		hal_spinlockClear(&spawn.sl, &sc);
	}
	else {
		vm_kfree(argv);
		vm_kfree(envp);
	}

	hal_spinlockDestroy(&spawn.sl);
	/* MISRAC2012-RULE_17_7-a */
	(void)vm_objectPut(spawn.object);
	return spawn.state < 0 ? spawn.state : pid;
}


int proc_fileSpawn(const char *path, char **argv, char **envp)
{
	int err;
	oid_t oid;
	vm_object_t *object;

	if ((err = proc_lookup(path, NULL, &oid)) < 0) {
		return err;
	}

	if ((err = vm_objectGet(&object, oid)) < 0) {
		return err;
	}

	return proc_spawn(object, NULL, NULL, NULL, 0, object->size, path, argv, envp);
}


int proc_syspageSpawnName(const char *imap, const char *dmap, const char *name, char **argv)
{
	const syspage_map_t *sysMap, *codeMap;
	const syspage_prog_t *prog = syspage_progNameResolve(name);
	vm_map_t *imapp = NULL;

	if (prog == NULL) {
		return -ENOENT;
	}

	sysMap = (dmap == NULL) ? syspage_mapIdResolve(prog->dmaps[0]) : syspage_mapNameResolve(dmap);

	if (imap != NULL) {
		codeMap = syspage_mapNameResolve(imap);
		if (codeMap == NULL) {
			return -EINVAL;
		}
	}
	else {
		codeMap = syspage_mapIdResolve(prog->imaps[0]);
	}

	if (codeMap != NULL) {
		if ((codeMap->attr & ((unsigned int)mAttrRead | (unsigned int)mAttrExec)) != ((unsigned int)mAttrRead | (unsigned int)mAttrExec)) {
			return -EINVAL;
		}

		imapp = vm_getSharedMap((int)codeMap->id);
	}

	if (sysMap != NULL && (sysMap->attr & ((unsigned int)mAttrRead | (unsigned int)mAttrWrite)) == ((unsigned int)mAttrRead | (unsigned int)mAttrWrite)) {
		return proc_syspageSpawn((syspage_prog_t *)prog, imapp, vm_getSharedMap((int)sysMap->id), name, argv);
	}

	return -EINVAL;
}


int proc_syspageSpawn(syspage_prog_t *program, vm_map_t *imap, vm_map_t *map, const char *path, char **argv)
{
	return proc_spawn(VM_OBJ_PHYSMEM, program, imap, map, (int)program->start, program->end - program->start, path, argv, NULL);
}


/* (v)fork/exec/exit */


static size_t process_parentKstacksz(thread_t *parent)
{
	return parent->kstacksz - (hal_cpuGetSP(parent->context) - (parent->kstack));
	// TBD_Julia można zrzutować w nawiasie z obu stron na unsigned int, ale powoduje błąd 11.6, bo są to voidy
}


static void process_restoreParentKstack(thread_t *current, thread_t *parent)
{
	hal_memcpy(hal_cpuGetSP(parent->context), current->parentkstack, process_parentKstacksz(parent));
	vm_kfree(current->parentkstack);
}


static void proc_vforkedExit(thread_t *current, process_spawn_t *spawn, int state)
{
	spinlock_ctx_t sc;

	current->ustack = NULL;
	proc_changeMap(current->process, NULL, NULL, NULL);

	/* Only possible in the case of `initthread` exit or failure to fork. */
	if (spawn->parent == NULL) {
		hal_spinlockDestroy(&spawn->sl);
		/* MISRAC2012-RULE_17_7-a */
		(void)vm_objectPut(spawn->object);
	}
	else {
		process_restoreParentKstack(current, spawn->parent);

		hal_spinlockSet(&spawn->sl, &sc);
		spawn->state = state;
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_threadWakeup(&spawn->wq);
		hal_spinlockClear(&spawn->sl, &sc);
	}

	proc_kill(current->process);
	proc_threadEnd();
}


void proc_exit(int code)
{
	thread_t *current = proc_current();
	process_spawn_t *spawn = current->execdata;
	arg_t args[3];

	current->process->exit = code;

	if (spawn != NULL) {
		hal_cpuDisableInterrupts();
		if (spawn->parent != NULL) {
			current->kstack = current->execkstack;
			_hal_cpuSetKernelStack(current->kstack + current->kstacksz);
		}

		args[0] = (arg_t)current;
		args[1] = (arg_t)spawn;
		args[2] = (arg_t)FORKED;
		hal_jmp(proc_vforkedExit, current->kstack + current->kstacksz, NULL, 3, args);
	}

	proc_kill(current->process);
}


static void process_vforkThread(void *arg)
{
	process_spawn_t *spawn = arg;
	thread_t *current, *parent;
	spinlock_ctx_t sc;
	int ret;

	current = proc_current();
	parent = spawn->parent;
	/* MISRAC2012-RULE_17_7-a */
	(void)posix_clone(process_getPid(parent->process));

	proc_changeMap(current->process, parent->process->mapp, parent->process->imapp, parent->process->pmapp);

	current->process->sigmask = parent->process->sigmask;
	current->process->sighandler = parent->process->sighandler;
	pmap_switch(current->process->pmapp);

	hal_spinlockSet(&spawn->sl, &sc);
	while (spawn->state < FORKING) {
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_threadWait(&spawn->wq, &spawn->sl, 0, &sc);
	}
	hal_spinlockClear(&spawn->sl, &sc);

	/* Copy parent kernel stack */
	current->parentkstack = vm_kmalloc(process_parentKstacksz(parent));
	if (current->parentkstack == NULL) {
		hal_spinlockSet(&spawn->sl, &sc);
		spawn->state = -ENOMEM;
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_threadWakeup(&spawn->wq);
		hal_spinlockClear(&spawn->sl, &sc);

		proc_threadEnd();
	}

	hal_memcpy(current->parentkstack, hal_cpuGetSP(parent->context), process_parentKstacksz(parent));

	current->execkstack = current->kstack;
	current->execdata = spawn;

	hal_memcpy(&current->process->tls, &parent->process->tls, sizeof(hal_tls_t));
	hal_memcpy(&current->tls, &parent->tls, sizeof(hal_tls_t));

	ret = proc_resourcesCopy(parent->process);
	if (ret < 0) {
		vm_kfree(current->parentkstack);

		hal_spinlockSet(&spawn->sl, &sc);
		spawn->state = ret;
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_threadWakeup(&spawn->wq);
		hal_spinlockClear(&spawn->sl, &sc);

		proc_threadEnd();
	}

	current->ustack = parent->ustack;

	hal_cpuDisableInterrupts();
	current->kstack = parent->kstack;
	_hal_cpuSetKernelStack(current->kstack + current->kstacksz);

	/* MISRA Rule 11.6: NULL is now a voind pointer!!!*/
	if (current->tls.tls_base != 0U) {
		hal_cpuTlsSet(&current->tls, current->context);
	}

	/* Start execution from parent suspend point */
	proc_longjmp(parent->context);

	/* This part of code left unexecuted */
	return;
}


int proc_vfork(void)
{
	thread_t *current;
	int pid, isparent = 1, ret;
	process_spawn_t *spawn;
	spinlock_ctx_t sc;

	current = proc_current();
	if (current == NULL) {
		return -EINVAL;
	}

	spawn = vm_kmalloc(sizeof(*spawn));
	if (spawn == NULL) {
		return -ENOMEM;
	}

	hal_spinlockCreate(&spawn->sl, "execsl");

	spawn->object = NULL;
	spawn->offset = 0;
	spawn->size = 0;
	spawn->wq = NULL;
	spawn->state = PREFORK;
	spawn->parent = current;
	spawn->prog = NULL;

	pid = proc_start(process_vforkThread, spawn, NULL);
	if (pid < 0) {
		hal_spinlockDestroy(&spawn->sl);
		vm_kfree(spawn);
		return pid;
	}

	/* Signal forking state to vfork thread */
	hal_spinlockSet(&spawn->sl, &sc);
	spawn->state = FORKING;
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_threadWakeup(&spawn->wq);

	do {
		/*
		 * proc_threadWait call stores context on the stack - this allows to execute child
		 * thread starting from this point
		 */
		/* MISRAC2012-RULE_17_7-a */
		(void)proc_threadWait(&spawn->wq, &spawn->sl, 0, &sc);
		isparent = (proc_current() == current) ? 1 : 0;
	} while ((spawn->state < FORKED) && (spawn->state > 0) && (isparent != 0));

	hal_spinlockClear(&spawn->sl, &sc);

	if (isparent != 0) {
		hal_spinlockDestroy(&spawn->sl);
		/* MISRAC2012-RULE_17_7-a */
		(void)vm_objectPut(spawn->object);
		ret = spawn->state;
		vm_kfree(spawn);
		return (ret < 0) ? ret : pid;
	}

	return 0;
}


#ifndef NOMMU
static int process_copy(void)
{
	thread_t *parent, *current = proc_current();
	process_spawn_t *spawn = current->execdata;
	process_t *process = current->process;
	parent = spawn->parent;

	process->path = lib_strdup(parent->process->path);
	if (process->path == NULL) {
		return -ENOMEM;
	}

	/* Avoid ustack access while map is invalid */
	current->ustack = NULL;

	vm_mapCreate(&process->map, parent->process->mapp->start, parent->process->mapp->stop);

	if (vm_mapCopy(process, &process->map, &parent->process->map) < 0) {
		return -ENOMEM;
	}

	proc_changeMap(process, &process->map, process->imapp, &process->map.pmap);

	current->ustack = parent->ustack;

	hal_cpuSmpSync();

	pmap_switch(process->pmapp);
	return EOK;
}
#endif


int proc_release(void)
{
	thread_t *current, *parent;
	process_spawn_t *spawn;
	spinlock_ctx_t sc;

	current = proc_current();

	spawn = current->execdata;
	if (spawn == NULL) {
		return -EINVAL;
	}

	parent = spawn->parent;
	if (parent == NULL) {
		return -EINVAL;
	}

	process_restoreParentKstack(current, parent);

	current->execdata = NULL;
	current->parentkstack = NULL;

	hal_spinlockSet(&spawn->sl, &sc);
	spawn->state = FORKED;
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_threadWakeup(&spawn->wq);
	hal_spinlockClear(&spawn->sl, &sc);

	return EOK;
}


int proc_fork(void)
{
	int err = -ENOSYS;
#ifndef NOMMU
	thread_t *current, *parent;
	unsigned sigmask;
	arg_t args[3];

	err = proc_vfork();
	if (err == 0) {
		current = proc_current();

		/* Mask all signals - during process_copy(), incoming signal might try
		 * to access our not-yet existent stack */
		sigmask = current->sigmask;
		current->sigmask = 0xffffffff;
		err = process_copy();
		current->sigmask = sigmask;

		hal_cpuDisableInterrupts();
		current->kstack = current->execkstack;
		_hal_cpuSetKernelStack(current->kstack + current->kstacksz);

		/* Copy parent context to child kstack in case of signal handling on fork return */
		parent = ((process_spawn_t *)current->execdata)->parent;
		hal_memcpy(current->kstack + current->kstacksz - sizeof(cpu_context_t),
				parent->kstack + parent->kstacksz - sizeof(cpu_context_t), sizeof(cpu_context_t));

		hal_cpuSmpSync();

		if (err < 0) {
			args[0] = (arg_t)current;
			args[1] = (arg_t)current->execdata;
			args[2] = (arg_t)err;
			hal_jmp(proc_vforkedExit, current->kstack + current->kstacksz, NULL, 3, args);
		}
		else {
			hal_cpuEnableInterrupts();
		}
	}
#endif
	return err;
}


static int process_execve(thread_t *current)
{
	process_spawn_t *spawn = current->execdata;
	thread_t *parent = spawn->parent;
	vm_map_t *map, *imap;

	/* The old user stack is no longer valid */
	current->ustack = NULL;

	/* Restore kernel stack of parent thread */
	if (parent != NULL) {
		process_restoreParentKstack(current, parent);
	}
	else {
		/* Reinitialize process */
		map = current->process->mapp;
		imap = current->process->imapp;
		proc_changeMap(current->process, NULL, NULL, NULL);
		pmap_switch(&process_common.kmap->pmap);

		vm_mapDestroy(current->process, map);

		if (imap != NULL) {
			vm_mapDestroy(current->process, imap);
		}

		proc_resourcesDestroy(current->process);
		proc_portsDestroy(current->process);
	}

	current->execkstack = NULL;
	current->parentkstack = NULL;
	current->execdata = NULL;

	current->process->sighandler = NULL;
	current->process->sigpend = 0;

	/* Close cloexec file descriptors */
	/* MISRAC2012-RULE_17_7-a */
	(void)posix_exec();
	process_exec(current, spawn);

	/* Not reached */
	return 0;
}


int proc_execve(const char *path, char **argv, char **envp)
{
	thread_t *current;
	char *kpath;
	process_spawn_t sspawn, *spawn;
	arg_t args[1];

	oid_t oid;
	vm_object_t *object;
	int err;

	current = proc_current();

	kpath = lib_strdup(path);
	if (kpath == NULL) {
		return -ENOMEM;
	}

	if (argv != NULL && (argv = proc_copyargs(argv)) == NULL) {
		vm_kfree(kpath);
		return -ENOMEM;
	}

	if (envp != NULL && (envp = proc_copyargs(envp)) == NULL) {
		vm_kfree(kpath);
		vm_kfree(argv);
		return -ENOMEM;
	}

	if ((err = proc_lookup(path, NULL, &oid)) < 0) {
		vm_kfree(kpath);
		vm_kfree(argv);
		vm_kfree(envp);
		return err;
	}

	if ((err = vm_objectGet(&object, oid)) < 0) {
		vm_kfree(kpath);
		vm_kfree(argv);
		vm_kfree(envp);
		return err;
	}

	if ((spawn = current->execdata) == NULL) {
		spawn = current->execdata = &sspawn;
		hal_spinlockCreate(&spawn->sl, "spawn");
		spawn->wq = NULL;
		spawn->state = FORKED;
		spawn->parent = NULL;
	}

	spawn->argv = argv;
	spawn->envp = envp;

	spawn->object = object;
	spawn->offset = 0;
	spawn->size = object->size;
	spawn->prog = NULL;

	vm_kfree(current->process->path);
	vm_kfree(current->process->envp);
	vm_kfree(current->process->argv);

	current->process->path = kpath;

	if (spawn->parent != NULL) {
		hal_cpuDisableInterrupts();
		current->kstack = current->execkstack;
		_hal_cpuSetKernelStack(current->kstack + current->kstacksz);

		args[0] = (arg_t)current;
		hal_jmp(process_execve, current->kstack + current->kstacksz, NULL, 1, args);
	}
	else {
		/* MISRAC2012-RULE_17_7-a */
		(void)process_execve(current);
	}
	/* Not reached */

	return 0;
}


int proc_sigpost(int pid, int sig)
{
	process_t *p;
	int err = -EINVAL;

	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockSet(&process_common.lock);
	p = lib_idtreeof(process_t, idlinkage, lib_idtreeFind(&process_common.id, pid));
	if (p != NULL) {
		err = threads_sigpost(p, NULL, sig);
	}
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockClear(&process_common.lock);

	return err;
}


int _process_init(vm_map_t *kmap, vm_object_t *kernel)
{
	process_common.kmap = kmap;
	process_common.first = NULL;
	process_common.kernel = kernel;
	process_common.idcounter = 1;
	/* MISRAC2012-RULE_17_7-a */
	(void)proc_lockInit(&process_common.lock, &proc_lockAttrDefault, "process.common");
	lib_idtreeInit(&process_common.id);

	/* MISRAC2012-RULE_17_7-a */
	(void)hal_exceptionsSetHandler(EXC_DEFAULT, process_exception);
	(void)hal_exceptionsSetHandler(EXC_UNDEFINED, process_illegal);
	return EOK;
}


int process_tlsInit(hal_tls_t *dest, hal_tls_t *source, vm_map_t *map)
{
	int err;
	dest->tdata_sz = source->tdata_sz;
	dest->tbss_sz = source->tbss_sz;
	dest->tls_sz = round_page(source->tls_sz);
	dest->arm_m_tls = source->arm_m_tls;

	/* MISRA Rule 11.6: Added (unsigned int *) */
	dest->tls_base = (ptr_t)(unsigned int *)vm_mmap(map, NULL, NULL, dest->tls_sz, PROT_READ | PROT_WRITE | PROT_USER, NULL, 0, MAP_NONE);

	/* MISRA Rule 11.6: NULL is now a voind pointer!!!*/
	if (dest->tls_base != 0U) {
		/* MISRA Rule 11.6: Added (unsigned int *) x2 */
		hal_memcpy((void *)(unsigned int *)dest->tls_base, (void *)(unsigned int *)source->tls_base, dest->tdata_sz);
		hal_memset((char *)dest->tls_base + dest->tdata_sz, 0, dest->tbss_sz);
		/* At the end of TLS there must be a pointer to itself */
		*(ptr_t *)((dest->tls_base + dest->tdata_sz + dest->tbss_sz + sizeof(ptr_t) - 1U) & ~(sizeof(ptr_t) - 1U)) = dest->tls_base + dest->tdata_sz + dest->tbss_sz;
		err = EOK;
	}
	else {
		err = -ENOMEM;
	}
	return err;
}


int process_tlsDestroy(hal_tls_t *tls, vm_map_t *map)
{
	/* MISRA Rule 11.6: Added (unsigned int *) */
	return vm_munmap(map, (void *)(unsigned int *)tls->tls_base, tls->tls_sz);
}
