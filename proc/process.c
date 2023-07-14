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

#include "../hal/hal.h"
#include "../include/errno.h"
#include "../include/signal.h"
#include "../vm/vm.h"
#include "../lib/lib.h"
#include "../posix/posix.h"
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
	offs_t offset;
	size_t size;
	vm_map_t *map;
	vm_map_t *imap;

	char **argv;
	char **envp;
} process_spawn_t;


struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
	process_t *first;
	size_t stacksz;
	lock_t lock;
	rbtree_t id;
	unsigned idcounter;
} process_common;


static int proc_idcmp(rbnode_t *n1, rbnode_t *n2)
{
	process_t *p1 = lib_treeof(process_t, idlinkage, n1);
	process_t *p2 = lib_treeof(process_t, idlinkage, n2);

	if (p1->id < p2->id)
		return -1;

	else if (p1->id > p2->id)
		return 1;

	return 0;
}


process_t *proc_find(unsigned pid)
{
	process_t *p, s;
	s.id = pid;

	proc_lockSet(&process_common.lock);
	if ((p = lib_treeof(process_t, idlinkage, lib_rbFind(&process_common.id, &s.idlinkage))) != NULL)
		p->refs++;
	proc_lockClear(&process_common.lock);

	return p;
}


static void process_destroy(process_t *p)
{
	thread_t *ghost;
	vm_map_t *mapp = p->mapp, *imapp = p->imapp;

	perf_kill(p);

	posix_died(p->id, p->exit);

	proc_changeMap(p, NULL, NULL, NULL);

	if (mapp != NULL) {
		vm_mapDestroy(p, mapp);
	}

	if (imapp != NULL) {
		vm_mapDestroy(p, imapp);
	}

	proc_resourcesDestroy(p);
	proc_portsDestroy(p);
	proc_lockDone(&p->lock);

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

	proc_lockSet(&process_common.lock);
	if (!(remaining = --p->refs))
		lib_rbRemove(&process_common.id, &p->idlinkage);
	proc_lockClear(&process_common.lock);

	if (!remaining)
		process_destroy(p);

	return remaining;
}


void proc_get(process_t *p)
{
	proc_lockSet(&process_common.lock);
	++p->refs;
	proc_lockClear(&process_common.lock);
}


static unsigned _process_alloc(unsigned id)
{
	process_t *p = lib_treeof(process_t, idlinkage, process_common.id.root);

	while (p != NULL) {
		if (p->lgap && id < p->id) {
			if (p->idlinkage.left == NULL)
				return max(id, p->id - p->lgap);

			p = lib_treeof(process_t, idlinkage, p->idlinkage.left);
			continue;
		}

		if (p->rgap) {
			if (p->idlinkage.right == NULL)
				return max(id, p->id + 1);

			p = lib_treeof(process_t, idlinkage, p->idlinkage.right);
			continue;
		}

		for (;; p = lib_treeof(process_t, idlinkage, p->idlinkage.parent)) {
			if (p->idlinkage.parent == NULL)
				return NULL;

			if ((p == lib_treeof(process_t, idlinkage, p->idlinkage.parent->left)) && lib_treeof(process_t, idlinkage, p->idlinkage.parent)->rgap)
				break;
		}
		p = lib_treeof(process_t, idlinkage, p->idlinkage.parent);

		if (p->idlinkage.right == NULL)
			return p->id + 1;

		p = lib_treeof(process_t, idlinkage, p->idlinkage.right);
	}

	return id;
}


static unsigned process_alloc(process_t *process)
{
	proc_lockSet(&process_common.lock);
	process->id = _process_alloc(process_common.idcounter);

	if (!process->id)
		process->id = _process_alloc(process_common.idcounter = 1);

	if (process_common.idcounter == MAX_PID)
		process_common.idcounter = 1;

	if (process->id) {
		lib_rbInsert(&process_common.id, &process->idlinkage);
		process_common.idcounter++;
	}
	proc_lockClear(&process_common.lock);

	return process->id;
}


static void process_augment(rbnode_t *node)
{
	rbnode_t *it;
	process_t *n = lib_treeof(process_t, idlinkage, node);
	process_t *p = n, *r, *l;

	if (node->left == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(process_t, idlinkage, it->parent);
			if (it->parent->right == it)
				break;
		}

		n->lgap = !!((n->id <= p->id) ? n->id : n->id - p->id - 1);
	}
	else {
		l = lib_treeof(process_t, idlinkage, node->left);
		n->lgap = max((int)l->lgap, (int)l->rgap);
	}

	if (node->right == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(process_t, idlinkage, it->parent);
			if (it->parent->left == it)
				break;
		}

		n->rgap = !!((n->id >= p->id) ? MAX_PID - n->id - 1 : p->id - n->id - 1);
	}
	else {
		r = lib_treeof(process_t, idlinkage, node->right);
		n->rgap = max((int)r->lgap, (int)r->rgap);
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(process_t, idlinkage, it);
		p = lib_treeof(process_t, idlinkage, it->parent);

		if (it->parent->left == it)
			p->lgap = max((int)n->lgap, (int)n->rgap);
		else
			p->rgap = max((int)n->lgap, (int)n->rgap);
	}
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
		process->path = vm_kmalloc(hal_strlen(path) + 1);
		if (process->path == NULL) {
			vm_kfree(process);
			return -ENOMEM;
		}

		hal_strcpy(process->path, path);
	}

	process->argv = NULL;
	process->envp = NULL;
	process->threads = NULL;
	process->ghosts = NULL;
	process->reaper = NULL;
	process->refs = 1;

	proc_lockInit(&process->lock, "process");

	process->ports = NULL;

	process->sigpend = 0;
	process->sigmask = 0;
	process->sighandler = NULL;
	process->tls.tls_base = NULL;
	process->tls.tbss_sz = 0;
	process->tls.tdata_sz = 0;
	process->tls.tls_sz = 0;
	process->tls.arm_m_tls = NULL;

#ifndef NOMMU
	process->lazy = 0;
#else
	process->lazy = 1;
#endif

	proc_changeMap(process, NULL, NULL, NULL);

	/* Initialize resources tree for mutex and cond handles */
	_resource_init(process);
	process_alloc(process);
	perf_fork(process);

	if (proc_threadCreate(process, initthr, NULL, 4, SIZE_KSTACK, NULL, 0, (void *)arg) < 0) {
		proc_put(process);
		return -EINVAL;
	}

	return process->id;
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

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

	posix_write(2, buff, hal_strlen(buff));
	posix_write(2, "\n", 1);

	/* use proc_current() as late as possible - to be able to print exceptions in scheduler */
	thread = proc_current();
	process = thread->process;

	if ((intr = userintr_active()) != NULL)
		lib_printf("in interrupt (%u) handler of process \"%s\" (PID: %u)\n", intr->handler.n, intr->process->path, intr->process->id);
	else if (process == NULL)
		lib_printf("in kernel thread %lu\n", thread->id);
	else
		lib_printf("in thread %lu, process \"%s\" (PID: %u)\n", thread->id, process->path, process->id);
}


void process_exception(unsigned int n, exc_context_t *ctx)
{
	thread_t *thread = proc_current();

	process_dumpException(n, ctx);

	if (thread->process == NULL)
		hal_cpuHalt();

	threads_sigpost(thread->process, thread, signal_kill);
	hal_cpuReschedule(NULL, NULL);
}


static void process_illegal(unsigned int n, exc_context_t *ctx)
{
	thread_t *thread = proc_current();
	process_t *process = thread->process;

	if (process == NULL)
		hal_cpuHalt();

	threads_sigpost(process, thread, signal_illegal);
}


static void process_tlsAssign(hal_tls_t *process_tls, hal_tls_t *tls, ptr_t tbssAddr)
{
	if (tls->tls_base != NULL) {
		process_tls->tls_base = tls->tls_base;
	}
	else if (tbssAddr != NULL) {
		process_tls->tls_base = tbssAddr;
	}
	else {
		process_tls->tls_base = NULL;
	}
	process_tls->tdata_sz = tls->tdata_sz;
	process_tls->tbss_sz = tls->tbss_sz;
	process_tls->tls_sz = tls->tbss_sz + tls->tdata_sz + sizeof(void *);
	process_tls->arm_m_tls = tls->arm_m_tls;
}


#ifndef NOMMU

/* TODO - adding error handling and unmapping of already mapped segments */
int process_load32(vm_map_t *map, vm_object_t *o, offs_t base, void *iehdr, size_t *ustacksz, hal_tls_t *tls, ptr_t *tbssAddr)
{
	void *vaddr = NULL;
	size_t memsz = 0, filesz = 0;
	Elf32_Ehdr *ehdr = iehdr;
	Elf32_Phdr *phdr;
	Elf32_Shdr *shdr;
	unsigned i, prot, flags, misalign = 0;
	offs_t offs;
	char *snameTab;

	shdr = (void *)((char *)ehdr + ehdr->e_shoff);
	shdr += ehdr->e_shstrndx;
	snameTab = (char *)ehdr + shdr->sh_offset;

	/* Find .tdata and .tbss sections */
	for (i = 0, shdr = (void *)((char *)ehdr + ehdr->e_shoff); i < ehdr->e_shnum; i++, shdr++) {
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
		if (phdr->p_type == PT_GNU_STACK && phdr->p_memsz != 0) {
			*ustacksz = round_page(phdr->p_memsz);
		}

		if (phdr->p_type != PT_LOAD || phdr->p_vaddr == 0)
			continue;

		vaddr = (void *)((unsigned long)phdr->p_vaddr & ~(phdr->p_align - 1));
		offs = phdr->p_offset & ~(phdr->p_align - 1);
		misalign = phdr->p_offset & (phdr->p_align - 1);
		filesz = phdr->p_filesz ? phdr->p_filesz + misalign : 0;
		memsz = phdr->p_memsz + misalign;

		prot = PROT_USER;
		flags = MAP_NONE;

		if (phdr->p_flags & PF_R)
			prot |= PROT_READ;

		if (phdr->p_flags & PF_W)
			prot |= PROT_WRITE;

		if (phdr->p_flags & PF_X)
			prot |= PROT_EXEC;

		if (filesz && (prot & PROT_WRITE))
			flags |= MAP_NEEDSCOPY;

		if (filesz && vm_mmap(map, vaddr, NULL, round_page(filesz), prot, o, base + offs, flags) == NULL)
			return -ENOMEM;

		if (filesz != memsz) {
			if (round_page(memsz) - round_page(filesz) && vm_mmap(map, vaddr, NULL, round_page(memsz) - round_page(filesz), prot, NULL, -1, MAP_NONE) == NULL)
				return -ENOMEM;

			hal_memset(vaddr + filesz, 0, round_page((unsigned long)vaddr + memsz) - (unsigned long)vaddr - filesz);
		}
	}
	return EOK;
}


int process_load64(vm_map_t *map, vm_object_t *o, offs_t base, void *iehdr, size_t *ustacksz, hal_tls_t *tls, ptr_t *tbssAddr)
{
	void *vaddr = NULL;
	size_t memsz = 0, filesz = 0;
	Elf64_Ehdr *ehdr = iehdr;
	Elf64_Phdr *phdr;
	Elf64_Shdr *shdr;
	unsigned i, prot, flags, misalign = 0;
	offs_t offs;
	char *snameTab;

	shdr = (void *)((char *)ehdr + ehdr->e_shoff);
	shdr += ehdr->e_shstrndx;
	snameTab = (char *)ehdr + shdr->sh_offset;

	/* Find .tdata and .tbss sections */
	for (i = 0, shdr = (void *)((char *)ehdr + ehdr->e_shoff); i < ehdr->e_shnum; i++, shdr++) {
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
		if (phdr->p_type == PT_GNU_STACK && phdr->p_memsz != 0) {
			*ustacksz = round_page(phdr->p_memsz);
		}

		if (phdr->p_type != PT_LOAD || phdr->p_vaddr == 0)
			continue;

		vaddr = (void *)((unsigned long)phdr->p_vaddr & ~(phdr->p_align - 1));
		offs = phdr->p_offset & ~(phdr->p_align - 1);
		misalign = phdr->p_offset & (phdr->p_align - 1);
		filesz = phdr->p_filesz ? phdr->p_filesz + misalign : 0;
		memsz = phdr->p_memsz + misalign;

		prot = PROT_USER;
		flags = MAP_NONE;

		if (phdr->p_flags & PF_R)
			prot |= PROT_READ;

		if (phdr->p_flags & PF_W)
			prot |= PROT_WRITE;

		if (phdr->p_flags & PF_X)
			prot |= PROT_EXEC;

		if (filesz && (prot & PROT_WRITE))
			flags |= MAP_NEEDSCOPY;

		if (filesz && vm_mmap(map, vaddr, NULL, round_page(filesz), prot, o, base + offs, flags) == NULL)
			return -ENOMEM;

		if (filesz != memsz) {
			if (round_page(memsz) - round_page(filesz) && vm_mmap(map, vaddr, NULL, round_page(memsz) - round_page(filesz), prot, NULL, -1, MAP_NONE) == NULL)
				return -ENOMEM;

			hal_memset(vaddr + filesz, 0, round_page((unsigned long)vaddr + memsz) - (unsigned long)vaddr - filesz);
		}
	}
	return EOK;
}


int process_load(process_t *process, vm_object_t *o, offs_t base, size_t size, void **ustack, void **entry)
{
	void *stack;
	Elf64_Ehdr *ehdr;
	vm_map_t *map = process->mapp;
	size_t ustacksz = SIZE_USTACK;
	int err;
	hal_tls_t tlsNew;
	ptr_t tbssAddr = 0;

	tlsNew.tls_base = NULL;
	tlsNew.tdata_sz = 0;
	tlsNew.tbss_sz = 0;
	tlsNew.tls_sz = 0;
	tlsNew.arm_m_tls = NULL;

	size = round_page(size);

	if ((ehdr = vm_mmap(process_common.kmap, NULL, NULL, size, PROT_READ, o, base, MAP_NONE)) == NULL)
		return -ENOMEM;

	/* Test ELF header */
	if (hal_strncmp((char *)ehdr->e_ident, "\177ELF", 4)) {
		vm_munmap(process_common.kmap, ehdr, size);
		return -ENOEXEC;
	}

	*entry = (void *)(unsigned long)ehdr->e_entry;

	err = EOK;

	switch (ehdr->e_ident[4]) {
		/* 32-bit binary */
		case 1:
			err = process_load32(map, o, base, ehdr, &ustacksz, &tlsNew, &tbssAddr);
			break;

		/* 64-bit binary */
		case 2:
			err = process_load64(map, o, base, ehdr, &ustacksz, &tlsNew, &tbssAddr);
			break;

		default:
			err = -ENOEXEC;
	}
	vm_munmap(process_common.kmap, ehdr, size);

	if (err < 0)
		return err;

	process_tlsAssign(&process->tls, &tlsNew, tbssAddr);

	/* Allocate and map user stack */
	if ((stack = vm_mmap(map, map->pmap.end - ustacksz, NULL, ustacksz, PROT_READ | PROT_WRITE | PROT_USER, NULL, -1, MAP_NONE)) == NULL) {
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

	if ((ptr_t)(*addr) == 0)
		return 0;

	for (i = 0; i < relocsz; ++i) {
		if ((ptr_t)reloc[i].vbase <= (ptr_t)(*addr) && (ptr_t)reloc[i].vbase + reloc[i].size > (ptr_t)(*addr)) {
			(*addr) = (void *)((ptr_t)(*addr) - (ptr_t)reloc[i].vbase + (ptr_t)reloc[i].pbase);
			return 0;
		}
	}

	return -1;
}


int process_load(process_t *process, vm_object_t *o, offs_t base, size_t size, void **ustack, void **entry)
{
	void *stack, *paddr;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	Elf32_Shdr *shdr;
#ifdef __sparc__
	Elf32_Rela *rela;
	Elf32_Sym *sym;
	ptr_t symTab;
#else
	Elf32_Rel *rel;
#endif
	unsigned prot, flags, reloffs;
	int i, j, relocsz = 0, reltype, badreloc = 0;
	void *relptr;
	char *snameTab;
	ptr_t *got;
	struct _reloc reloc[8];
	size_t stacksz = SIZE_USTACK;
	hal_tls_t tlsNew;
	ptr_t tbssAddr = 0;

	if (o != (void *)-1)
		return -ENOEXEC;

	ehdr = (void *)(ptr_t)base;

	/* Test ELF header */
	if (hal_strncmp((char *)ehdr->e_ident, "\177ELF", 4) || ehdr->e_shnum == 0)
		return -ENOEXEC;

	hal_memset(reloc, 0, sizeof(reloc));

	for (i = 0, j = 0, phdr = (void *)ehdr + ehdr->e_phoff; i < ehdr->e_phnum; i++, phdr++) {

		if (phdr->p_type == PT_GNU_STACK && phdr->p_memsz != 0) {
			stacksz = round_page(phdr->p_memsz);
		}

		if (phdr->p_type != PT_LOAD)
			continue;

		reloffs = 0;
		prot = PROT_USER;
		flags = MAP_NONE;
		paddr = (char *)ehdr + phdr->p_offset;

		if (phdr->p_flags & PF_R)
			prot |= PROT_READ;

		if (phdr->p_flags & PF_X) {
			prot |= PROT_EXEC;

			if (process->imapp != NULL &&
					(((ptr_t)base < (ptr_t)process->imapp->start) ||
					((ptr_t)base > (ptr_t)process->imapp->stop))) {
				paddr = vm_mmap(process->imapp, NULL, NULL, round_page(phdr->p_memsz), prot, NULL, -1, flags);
				if (paddr == NULL) {
					return -ENOMEM;
				}

				hal_memcpy((char *)paddr, (char *)ehdr + phdr->p_offset, phdr->p_memsz);

				/* Need to make cache and memory coherent, so $I is coherent too */
				hal_cleanDCache((ptr_t)paddr, phdr->p_memsz);
			}
		}

		if (phdr->p_flags & PF_W) {
			prot |= PROT_WRITE;

			reloffs = phdr->p_vaddr % SIZE_PAGE;

			if ((paddr = vm_mmap(process->mapp, NULL, NULL, round_page(phdr->p_memsz + reloffs), prot, NULL, -1, flags)) == NULL)
				return -ENOMEM;

			if (phdr->p_filesz) {
				if (phdr->p_offset + round_page(phdr->p_filesz) > size)
					return -ENOEXEC;

				hal_memcpy((char *)paddr + reloffs, (char *)ehdr + phdr->p_offset, phdr->p_filesz);
			}

			hal_memset((char *)paddr, 0, reloffs);
			hal_memset((char *)paddr + reloffs + phdr->p_filesz, 0, round_page(phdr->p_memsz + reloffs) - phdr->p_filesz - reloffs);
		}

		if (j >= sizeof(reloc) / sizeof(reloc[0]))
			return -ENOMEM;

		reloc[j].vbase = (void *)phdr->p_vaddr;
		reloc[j].pbase = (void *)((char *)paddr + reloffs);
		reloc[j].size = phdr->p_memsz;
		reloc[j].misalign = phdr->p_offset & (phdr->p_align - 1);
		++relocsz;
		++j;
	}

	shdr = (void *)((char *)ehdr + ehdr->e_shoff);
	shdr += ehdr->e_shstrndx;

	snameTab = (char *)ehdr + shdr->sh_offset;

	/* Find .got section */
	for (i = 0, shdr = (void *)((char *)ehdr + ehdr->e_shoff); i < ehdr->e_shnum; i++, shdr++) {
		if (hal_strcmp(&snameTab[shdr->sh_name], ".got") == 0)
			break;
	}

	if (i >= ehdr->e_shnum)
		return -ENOEXEC;

	got = (ptr_t *)shdr->sh_addr;
	if (process_relocate(reloc, relocsz, (char **)&got) < 0)
		return -ENOEXEC;

	/* Perform .got relocations */
	/* This is non classic approach to .got relocation. We use .got itselft
	 * instead of .rel section. */
	for (i = 0; i < shdr->sh_size / 4; ++i) {
		if (process_relocate(reloc, relocsz, (char **)&got[i]) < 0)
			return -ENOEXEC;
	}

	*entry = (void *)(unsigned long)ehdr->e_entry;
	if (process_relocate(reloc, relocsz, (char **)entry) < 0)
		return -ENOEXEC;

#ifdef __sparc__
	/* find symtab */
	for (i = 0, shdr = (void *)((char *)ehdr + ehdr->e_shoff); i < ehdr->e_shnum; i++, shdr++) {
		if (hal_strcmp(&snameTab[shdr->sh_name], ".symtab") == 0)
			break;
	}

	if (i >= ehdr->e_shnum) {
		return -ENOEXEC;
	}
	symTab = (ptr_t)ehdr + (ptr_t)shdr->sh_offset;

	/* Perform data, init_array and fini_array relocation */
	for (i = 0, shdr = (void *)((char *)ehdr + ehdr->e_shoff); i < ehdr->e_shnum; i++, shdr++) {
		if (hal_strncmp(&snameTab[shdr->sh_name], ".rela", 5) != 0)
			continue;

		if (!shdr->sh_size || !shdr->sh_entsize)
			continue;

		for (j = 0; j < shdr->sh_size / shdr->sh_entsize; ++j) {
			rela = (Elf32_Rela *)((ptr_t)shdr->sh_offset + (ptr_t)ehdr + (j * shdr->sh_entsize));
			reltype = ELF32_R_TYPE(rela->r_info);

			if (reltype == R_SPARC_32) {
				relptr = (void *)rela->r_offset;
				if (process_relocate(reloc, relocsz, (char **)&relptr) < 0) {
					return -ENOEXEC;
				}

				/* Don't modify ELF file! */
				if ((ptr_t)relptr >= (ptr_t)base && (ptr_t)relptr < (ptr_t)base + size) {
					++badreloc;
					continue;
				}

				sym = (Elf32_Sym *)(symTab + ELF32_R_SYM(rela->r_info) * sizeof(Elf32_Sym));

				/* Write addend to the address */
				*(char **)relptr = (char *)(sym->st_value + rela->r_addend);

				if (process_relocate(reloc, relocsz, relptr) < 0) {
					return -ENOEXEC;
				}
			}
		}
	}
#else
	/* Perform data, init_array and fini_array relocation */
	for (i = 0, shdr = (void *)((char *)ehdr + ehdr->e_shoff); i < ehdr->e_shnum; i++, shdr++) {
		if (hal_strncmp(&snameTab[shdr->sh_name], ".rel", 4) != 0)
			continue;

		if (!shdr->sh_size || !shdr->sh_entsize)
			continue;

		for (j = 0; j < shdr->sh_size / shdr->sh_entsize; ++j) {
			rel = (void *)((ptr_t)shdr->sh_offset + (ptr_t)ehdr + (j * shdr->sh_entsize));
			reltype = ELF32_R_TYPE(rel->r_info);

			if (reltype == R_ARM_ABS32 || reltype == R_ARM_TARGET1) {
				relptr = (void *)rel->r_offset;

				if (process_relocate(reloc, relocsz, (char **)&relptr) < 0)
					return -ENOEXEC;

				/* Don't modify ELF file! */
				if ((ptr_t)relptr >= (ptr_t)base && (ptr_t)relptr < (ptr_t)base + size) {
					++badreloc;
					continue;
				}

				if (process_relocate(reloc, relocsz, relptr) < 0)
					return -ENOEXEC;
			}
		}
	}
#endif

	tlsNew.tls_base = NULL;
	tlsNew.tdata_sz = 0;
	tlsNew.tbss_sz = 0;
	tlsNew.tls_sz = 0;
	tlsNew.arm_m_tls = NULL;

	/* Perform .tdata, .tbss and .armtls relocations */
	for (i = 0, shdr = (void *)((char *)ehdr + ehdr->e_shoff); i < ehdr->e_shnum; i++, shdr++) {
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
	if ((stack = vm_mmap(process->mapp, NULL, NULL, stacksz, PROT_READ | PROT_WRITE | PROT_USER, NULL, -1, MAP_NONE)) == NULL)
		return -ENOMEM;

	process->got = (void *)got;
	*ustack = stack + stacksz;

	threads_canaryInit(proc_current(), stack);

	if (badreloc) {
		if (process->path != NULL && process->path[0] != '\0')
			lib_printf("app %s: ", process->path);
		else
			lib_printf("process %d: ", process->id);

		lib_printf("Found %d badreloc%c\n", badreloc, (badreloc > 1) ? 's' : ' ');
	}

	return EOK;
}

#endif

void *proc_copyargs(char **args)
{
	int argc, len = 0;
	void *storage;
	char **kargs, *p;

	if (args == NULL)
		return NULL;

	for (argc = 0; args[argc] != NULL; ++argc)
		len += hal_strlen(args[argc]) + 1;

	len += (argc + 1) * sizeof(char *);

	if ((kargs = storage = vm_kmalloc(len)) == NULL)
		return NULL;

	kargs[argc] = NULL;

	p = (char *)storage + (argc + 1) * sizeof(char *);

	while (argc-- > 0) {
		len = hal_strlen(args[argc]) + 1;
		hal_memcpy(p, args[argc], len);
		kargs[argc] = p;
		p += len;
	}

	return storage;
}


static void *process_putargs(void *stack, char ***argsp, int *count)
{
	int argc, len;
	char **args_stack, **args = *argsp;

	if (args == NULL) {
		*count = 0;
		return stack;
	}

	for (argc = 0; args[argc] != NULL; ++argc)
		;

	stack -= (argc + 1) * sizeof(char *);
	args_stack = stack;
	args_stack[argc] = NULL;

	for (argc = 0; args[argc] != NULL; ++argc) {
		len = hal_strlen(args[argc]) + 1;
		stack -= (len + sizeof(long) - 1) & ~(sizeof(long) - 1);
		hal_memcpy(stack, args[argc], len);
		args_stack[argc] = stack;
	}

	*argsp = args_stack;
	*count = argc;

	return stack;
}


static void process_exec(thread_t *current, process_spawn_t *spawn)
{
	void *stack, *entry;
	int err, count;
	spinlock_ctx_t sc;

	current->process->argv = spawn->argv;
	current->process->envp = spawn->envp;

#ifndef NOMMU
	vm_mapCreate(&current->process->map, (void *)(VADDR_MIN + SIZE_PAGE), (void *)VADDR_USR_MAX);
	proc_changeMap(current->process, &current->process->map, NULL, &current->process->map.pmap);
#else
	proc_changeMap(current->process, (spawn->map != NULL) ? spawn->map : process_common.kmap, spawn->imap, &current->process->map.pmap);

	current->process->entries = NULL;
#endif

	pmap_switch(current->process->pmapp);

	err = process_load(current->process, spawn->object, spawn->offset, spawn->size, &stack, &entry);
	if (!err) {
		stack = process_putargs(stack, &spawn->envp, &count);
		stack = process_putargs(stack, &spawn->argv, &count);

		/* temporary? put arguments to main on stack */
		PUTONSTACK(stack, char **, spawn->envp);
		PUTONSTACK(stack, char **, spawn->argv);
		PUTONSTACK(stack, int, count);
		PUTONSTACK(stack, void *, NULL); /* return address */
	}

	if (spawn->parent == NULL) {
		/* if execing without vfork */
		hal_spinlockDestroy(&spawn->sl);
		vm_objectPut(spawn->object);
	}
	else {
		hal_spinlockSet(&spawn->sl, &sc);
		spawn->state = FORKED;
		proc_threadWakeup(&spawn->wq);
		hal_spinlockClear(&spawn->sl, &sc);
	}

	if (err == EOK && current->process->tls.tls_base != NULL) {
		err = process_tlsInit(&current->tls, &current->process->tls, current->process->mapp);
	}

	if (err < 0) {
		current->process->exit = err;
		proc_threadEnd();
	}

	hal_cpuDisableInterrupts();
	_hal_cpuSetKernelStack(current->kstack + current->kstacksz);
	hal_cpuSetGot(current->process->got);

	if (current->tls.tls_base != NULL) {
		hal_cpuTlsSet(&current->tls, current->context);
	}

#ifdef __TARGET_RISCV64
	hal_jmp(entry, current->kstack + current->kstacksz, stack, 3);
#else
	hal_jmp(entry, current->kstack + current->kstacksz, stack, 0);
#endif
}


static void proc_spawnThread(void *arg)
{
	thread_t *current = proc_current();
	process_spawn_t *spawn = arg;

	/* temporary: create new posix process */
	if (spawn->parent != NULL) {
		posix_clone(spawn->parent->process->id);
	}

	process_exec(current, spawn);
}


int proc_spawn(vm_object_t *object, vm_map_t *imap, vm_map_t *map, offs_t offset, size_t size, const char *path, char **argv, char **envp)
{
	int pid;
	process_spawn_t spawn;
	spinlock_ctx_t sc;

	if (argv != NULL && (argv = proc_copyargs(argv)) == NULL)
		return -ENOMEM;

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

	hal_spinlockCreate(&spawn.sl, "spawnsl");

	if ((pid = proc_start(proc_spawnThread, &spawn, path)) > 0) {
		hal_spinlockSet(&spawn.sl, &sc);
		while (spawn.state == FORKING)
			proc_threadWait(&spawn.wq, &spawn.sl, 0, &sc);
		hal_spinlockClear(&spawn.sl, &sc);
	}
	else {
		vm_kfree(argv);
		vm_kfree(envp);
	}

	hal_spinlockDestroy(&spawn.sl);
	vm_objectPut(spawn.object);
	return spawn.state < 0 ? spawn.state : pid;
}


int proc_fileSpawn(const char *path, char **argv, char **envp)
{
	int err;
	oid_t oid;
	vm_object_t *object;

	if ((err = proc_lookup(path, NULL, &oid)) < 0)
		return err;

	if ((err = vm_objectGet(&object, oid)) < 0)
		return err;

	return proc_spawn(object, NULL, NULL, 0, object->size, path, argv, envp);
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
	codeMap = (imap == NULL) ? syspage_mapIdResolve(prog->imaps[0]) : syspage_mapNameResolve(imap);

	if (codeMap != NULL) {
		if ((codeMap->attr & (mAttrRead | mAttrExec)) != (mAttrRead | mAttrExec)) {
			return -EINVAL;
		}

		imapp = vm_getSharedMap(codeMap->id);
	}

	if (sysMap != NULL && (sysMap->attr & (mAttrRead | mAttrWrite)) == (mAttrRead | mAttrWrite)) {
		return proc_syspageSpawn((syspage_prog_t *)prog, imapp, vm_getSharedMap(sysMap->id), name, argv);
	}

	return -EINVAL;
}


int proc_syspageSpawn(syspage_prog_t *program, vm_map_t *imap, vm_map_t *map, const char *path, char **argv)
{
	return proc_spawn((void *)-1, imap, map, program->start, program->end - program->start, path, argv, NULL);
}


/* (v)fork/exec/exit */


static void proc_vforkedExit(thread_t *current, process_spawn_t *spawn, int state)
{
	thread_t *parent = spawn->parent;
	spinlock_ctx_t sc;

	hal_memcpy(hal_cpuGetSP(parent->context), current->parentkstack + (hal_cpuGetSP(parent->context) - parent->kstack), parent->kstack + parent->kstacksz - hal_cpuGetSP(parent->context));
	vm_kfree(current->parentkstack);

	proc_changeMap(current->process, NULL, NULL, NULL);

	if (spawn->parent == NULL) {
		hal_spinlockDestroy(&spawn->sl);
		vm_objectPut(spawn->object);
	}
	else {
		hal_spinlockSet(&spawn->sl, &sc);
		spawn->state = state;
		proc_threadWakeup(&spawn->wq);
		hal_spinlockClear(&spawn->sl, &sc);
	}

	proc_kill(current->process);
	proc_threadEnd();
}


void proc_exit(int code)
{
	thread_t *current = proc_current();
	process_spawn_t *spawn;
	void *kstack;

	current->process->exit = code;

	if ((spawn = current->execdata) != NULL) {
		kstack = current->kstack = current->execkstack;
		kstack += current->kstacksz;

		PUTONSTACK(kstack, int, FORKED);
		PUTONSTACK(kstack, process_spawn_t *, spawn);
		PUTONSTACK(kstack, thread_t *, current);
		hal_jmp(proc_vforkedExit, kstack, NULL, 3);
	}

	proc_kill(current->process);
}


static void process_vforkThread(void *arg)
{
	process_spawn_t *spawn = arg;
	thread_t *current, *parent;
	spinlock_ctx_t sc;

	current = proc_current();
	parent = spawn->parent;
	posix_clone(parent->process->id);

	proc_changeMap(current->process, parent->process->mapp, parent->process->imapp, parent->process->pmapp);

	current->process->sigmask = parent->process->sigmask;
	current->process->sighandler = parent->process->sighandler;
	pmap_switch(current->process->pmapp);

	hal_spinlockSet(&spawn->sl, &sc);
	while (spawn->state < FORKING) {
		proc_threadWait(&spawn->wq, &spawn->sl, 0, &sc);
	}
	hal_spinlockClear(&spawn->sl, &sc);

	/* Copy parent kernel stack */
	current->parentkstack = vm_kmalloc(parent->kstacksz);
	if (current->parentkstack == NULL) {
		hal_spinlockSet(&spawn->sl, &sc);
		spawn->state = -ENOMEM;
		proc_threadWakeup(&spawn->wq);
		hal_spinlockClear(&spawn->sl, &sc);

		proc_threadEnd();
	}

	hal_memcpy(current->parentkstack + (hal_cpuGetSP(parent->context) - parent->kstack), hal_cpuGetSP(parent->context), parent->kstack + parent->kstacksz - hal_cpuGetSP(parent->context));

	current->execkstack = current->kstack;
	current->execdata = spawn;
	current->kstack = parent->kstack;

	hal_memcpy(&current->process->tls, &parent->process->tls, sizeof(hal_tls_t));
	hal_memcpy(&current->tls, &parent->tls, sizeof(hal_tls_t));

	hal_cpuDisableInterrupts();
	_hal_cpuSetKernelStack(current->kstack + current->kstacksz);

	if (current->tls.tls_base != NULL) {
		hal_cpuTlsSet(&current->tls, current->context);
	}

	/* Start execution from parent suspend point */
	hal_longjmp(parent->context);

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

	pid = proc_start(process_vforkThread, spawn, NULL);
	if (pid < 0) {
		hal_spinlockDestroy(&spawn->sl);
		vm_kfree(spawn);
		return pid;
	}

	/* Signal forking state to vfork thread */
	hal_spinlockSet(&spawn->sl, &sc);
	spawn->state = FORKING;
	proc_threadWakeup(&spawn->wq);

	do {
		/*
		 * proc_threadWait call stores context on the stack - this allows to execute child
		 * thread starting from this point
		 */
		proc_threadWait(&spawn->wq, &spawn->sl, 0, &sc);
		isparent = proc_current() == current;
	} while ((spawn->state < FORKED) && (spawn->state > 0) && (isparent != 0));

	hal_spinlockClear(&spawn->sl, &sc);

	if (isparent) {
		hal_spinlockDestroy(&spawn->sl);
		vm_objectPut(spawn->object);
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
	int len;

	len = hal_strlen(parent->process->path) + 1;

	if ((process->path = vm_kmalloc(len)) == NULL)
		return -ENOMEM;

	hal_memcpy(process->path, parent->process->path, len);

	if (proc_resourcesCopy(parent->process) < 0)
		return -ENOMEM;

	vm_mapCreate(&process->map, parent->process->mapp->start, parent->process->mapp->stop);

	if (vm_mapCopy(process, &process->map, &parent->process->map) < 0)
		return -ENOMEM;

	proc_changeMap(process, &process->map, process->imapp, &process->map.pmap);

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

	if ((spawn = current->execdata) == NULL)
		return -EINVAL;

	if ((parent = spawn->parent) == NULL)
		return -EINVAL;

	hal_memcpy(hal_cpuGetSP(parent->context), current->parentkstack + (hal_cpuGetSP(parent->context) - parent->kstack), parent->kstack + parent->kstacksz - hal_cpuGetSP(parent->context));
	vm_kfree(current->parentkstack);

	current->execdata = NULL;
	current->parentkstack = NULL;

	hal_spinlockSet(&spawn->sl, &sc);
	spawn->state = FORKED;
	proc_threadWakeup(&spawn->wq);
	hal_spinlockClear(&spawn->sl, &sc);

	return EOK;
}


int proc_fork(void)
{
	int err = -ENOSYS;
#ifndef NOMMU
	thread_t *current;
	void *kstack;
	unsigned sigmask;

	err = proc_vfork();
	if (err == 0) {
		current = proc_current();

		/* Mask all signals - during process_copy(), incoming signal might try
		 * to access our not-yet existent stack */
		sigmask = current->sigmask;
		current->sigmask = 0xffffffff;
		err = process_copy();
		current->sigmask = sigmask;

		current->kstack = current->execkstack;
		_hal_cpuSetKernelStack(current->kstack + current->kstacksz);

		if (err < 0) {
			kstack = current->kstack + current->kstacksz;
			PUTONSTACK(kstack, int, err);
			PUTONSTACK(kstack, process_spawn_t *, current->execdata);
			PUTONSTACK(kstack, thread_t *, current);
			hal_jmp(proc_vforkedExit, kstack, NULL, 3);
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
		hal_memcpy(hal_cpuGetSP(parent->context), current->parentkstack + (hal_cpuGetSP(parent->context) - parent->kstack), parent->kstack + parent->kstacksz - hal_cpuGetSP(parent->context));
		vm_kfree(current->parentkstack);
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
	posix_exec();
	process_exec(current, spawn);

	/* Not reached */
	return 0;
}


int proc_execve(const char *path, char **argv, char **envp)
{
	int len;
	thread_t *current;
	char *kpath;
	void *kstack;
	process_spawn_t sspawn, *spawn;

	oid_t oid;
	vm_object_t *object;
	int err;

	current = proc_current();

	len = hal_strlen(path) + 1;

	if ((kpath = vm_kmalloc(len)) == NULL)
		return -ENOMEM;

	hal_memcpy(kpath, path, len);

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

	vm_kfree(current->process->path);
	vm_kfree(current->process->envp);
	vm_kfree(current->process->argv);

	current->process->path = kpath;

	if (spawn->parent != NULL) {
		kstack = current->kstack = current->execkstack;
		kstack += current->kstacksz;

		PUTONSTACK(kstack, thread_t *, current);
		hal_jmp(process_execve, kstack, NULL, 1);
	}
	else {
		process_execve(current);
	}
	/* Not reached */

	return 0;
}


int proc_sigpost(int pid, int sig)
{
	process_t s, *p;
	int err;

	s.id = pid;

	proc_lockSet(&process_common.lock);
	if ((p = lib_treeof(process_t, idlinkage, lib_rbFind(&process_common.id, &s.idlinkage))) != NULL)
		err = threads_sigpost(p, NULL, sig);
	else
		err = -EINVAL;
	proc_lockClear(&process_common.lock);

	return err;
}


int _process_init(vm_map_t *kmap, vm_object_t *kernel)
{
	process_common.kmap = kmap;
	process_common.first = NULL;
	process_common.kernel = kernel;
	process_common.idcounter = 1;
	proc_lockInit(&process_common.lock, "process.common");
	lib_rbInit(&process_common.id, proc_idcmp, process_augment);

	hal_exceptionsSetHandler(EXC_DEFAULT, process_exception);
	hal_exceptionsSetHandler(EXC_UNDEFINED, process_illegal);
	return EOK;
}


int process_tlsInit(hal_tls_t *dest, hal_tls_t *source, vm_map_t *map)
{
	int err;
	dest->tdata_sz = source->tdata_sz;
	dest->tbss_sz = source->tbss_sz;
	dest->tls_sz = round_page(source->tls_sz);
	dest->arm_m_tls = source->arm_m_tls;

	dest->tls_base = (ptr_t)vm_mmap(map, NULL, NULL, dest->tls_sz, PROT_READ | PROT_WRITE | PROT_USER, NULL, 0, MAP_NONE);

	if (dest->tls_base != NULL) {
		hal_memcpy((void *)dest->tls_base, (void *)source->tls_base, dest->tdata_sz);
		hal_memset((char *)dest->tls_base + dest->tdata_sz, 0, dest->tbss_sz);
		/* At the end of TLS there must be a pointer to itself */
		*(ptr_t *)(dest->tls_base + dest->tdata_sz + dest->tbss_sz) = dest->tls_base + dest->tdata_sz + dest->tbss_sz;
		err = EOK;
	}
	else {
		err = -ENOMEM;
	}
	return err;
}


int process_tlsDestroy(hal_tls_t *tls, vm_map_t *map)
{
	return vm_munmap(map, (void *)tls->tls_base, tls->tls_sz);
}
