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

#include HAL
#include "../../include/errno.h"
#include "../../include/signal.h"
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


struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
	process_t *first;
	size_t stacksz;
	lock_t lock;
	rbtree_t id;
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


process_t *proc_find(unsigned int pid)
{
	process_t *p, s;
	s.id = pid;

	proc_lockSet(&process_common.lock);
	p = lib_treeof(process_t, idlinkage, lib_rbFind(&process_common.id, &s.idlinkage));
	proc_lockClear(&process_common.lock);

	return p;
}


int proc_start(void (*initthr)(void *), void *arg, const char *path)
{
	process_t *process;
#ifndef NOMMU
	page_t *p;
#endif

	if ((process = (process_t *)vm_kmalloc(sizeof(process_t))) == NULL)
		return -ENOMEM;

#ifdef NOMMU
	process->entries = NULL;
#endif

	process->id = 1;
	process->state = NORMAL;

	if ((process->path = vm_kmalloc(hal_strlen(path) + 1)) == NULL) {
		vm_kfree(process);
		return -ENOMEM;
	}

	hal_strcpy(process->path, path);
	process->parent = NULL;
	process->children = NULL;
	process->threads = NULL;

	process->waitq = NULL;
	process->waitpid = 0;

	proc_lockInit(&process->lock);

	/*process->uid = 0;
	process->euid = 0;
	process->suid = 0;
	process->gid = 0;
	process->egid = 0;
	process->sgid = 0;

	process->umask = 0x1ff;
	process->ports = NULL;*/

	process->ports = NULL;
	process->zombies = NULL;
	process->ghosts = NULL;
	process->gwaitq = NULL;
	process->waittid = 0;

	process->sigpend = 0;
	process->sigmask = 0;
	process->sighandler = NULL;

#ifndef NOMMU
	process->lazy = 0;
	process->mapp = &process->map;

	vm_mapCreate(process->mapp, (void *)VADDR_MIN + SIZE_PAGE, (void *)VADDR_USR_MAX);

	/* Create pmap */
	process->pmapp = p = vm_pageAlloc(SIZE_PDIR, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE);
	process->pmapv = vm_mmap(process_common.kmap, process_common.kmap->start, p, 1 << p->idx, PROT_READ | PROT_WRITE, process_common.kernel, -1, MAP_NONE);

	pmap_create(&process->mapp->pmap, &process_common.kmap->pmap, p, process->pmapv);
#else
	process->lazy = 1;
	process->mapp = process_common.kmap;
	//stack = (void *)VADDR_MIN;
#endif

	/* Initialize resources tree for mutex and cond handles */
	resource_init(process);

	if (proc_threadCreate(process, (void *)initthr, NULL, 4, SIZE_KSTACK, NULL, 0, (void *)arg) < 0) {
//		proc_threadDestroy();
		vm_kfree(process);
		return -EINVAL;
	}

	proc_lockSet(&process_common.lock);
	lib_rbInsert(&process_common.id, &process->idlinkage);
	proc_lockClear(&process_common.lock);

	return 0;
}


void proc_kill(process_t *proc)
{
	process_t *child, *zombie, *init;

	init = proc_find(1);

	proc_lockSet2(&init->lock, &proc->lock);
	if ((child = proc->children) != NULL) {
		do
			child->parent = init;
		while ((child = child->next) != proc->children);

		proc->children = NULL;

		if (init->children == NULL) {
			init->children = child;
		}
		else {
			swap(init->children->next, child->prev->next);
			swap(child->prev->next->prev, child->prev);
		}
	}

	if ((zombie = proc->zombies) != NULL) {
		proc->zombies = NULL;

		if (init->zombies == NULL) {
			init->zombies = zombie;
		}
		else {
			swap(init->zombies->next, zombie->prev->next);
			swap(zombie->prev->next->prev, zombie->prev);
		}

		proc_threadWakeup(&init->waitq);
	}
	proc_lockClear(&init->lock);
	proc_lockClear(&proc->lock);

	proc_lockSet(&process_common.lock);
	lib_rbRemove(&process_common.id, &proc->idlinkage);
	proc_lockClear(&process_common.lock);

	proc_threadsDestroy(proc);
	proc_resourcesFree(proc);
	proc_portsDestroy(proc);
	posix_exit(proc);

	if (proc == proc_current()->process)
		proc_threadDestroy();
}


void process_dumpException(unsigned int n, exc_context_t *ctx)
{
	thread_t *thread = proc_current();
	process_t *process = thread->process;
	intr_handler_t *intr;
	char buff[SIZE_CTXDUMP];

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);
	hal_consolePrint(ATTR_BOLD, "\n");

	if ((intr = userintr_active()) != NULL)
		lib_printf("in interrupt (%d) handler of process \"%s\" (%x)\n", intr->n, intr->process->path, intr->process->id);
	else if (process == NULL)
		lib_printf("in kernel thread %x\n", thread->id);
	else
		lib_printf("in thread %x, process \"%s\" (%x)\n", thread->id, process->path, process->id);
}


void process_exception(unsigned int n, exc_context_t *ctx)
{
	thread_t *thread = proc_current();

	process_dumpException(n, ctx);

	if (thread->process == NULL)
		hal_cpuHalt();

	proc_sigpost(thread->process, thread, signal_kill);
	hal_cpuReschedule(NULL);
}


static void process_illegal(unsigned int n, exc_context_t *ctx)
{
	thread_t *thread = proc_current();
	process_t *process = thread->process;

	if (process == NULL)
		hal_cpuHalt();

	proc_sigpost(process, thread, signal_illegal);

	if (!_proc_sigwant(thread))
		proc_sigpost(process, thread, signal_kill);

	hal_cpuReschedule(NULL);
}


static void process_exexepilogue(int exec, thread_t *current, thread_t *parent, void *entry, void *stack)
{
	cpu_context_t *ctx;
	hal_cpuGuard(NULL, current->kstack);

	if (parent != NULL) {
		if (current->parentkstack != NULL) {
			current->execfl = OWNSTACK;

			/* Restore kernel stack of parent thread */
			hal_memcpy(hal_cpuGetSP(parent->context), current->parentkstack + (hal_cpuGetSP(parent->context) - parent->kstack), parent->kstack + parent->kstacksz - hal_cpuGetSP(parent->context));
			vm_kfree(current->parentkstack);
			current->parentkstack = NULL;
		}

		/* Continue parent thread */
		hal_spinlockSet(&parent->execwaitsl);
		parent->execfl = exec < 0 ? NOFORK : FORKED;
		current->execparent = NULL;
		proc_threadWakeup(&parent->execwaitq);
		hal_spinlockClear(&parent->execwaitsl);
	}

	if (exec <= 0) {
		/* Exit process */
		proc_kill(current->process);
	}

	_hal_cpuSetKernelStack(current->kstack + current->kstacksz);
	proc_threadUnprotect();

	/* Switch from kernel mode to user mode */
	if (exec > 1) {
		ctx = (cpu_context_t *)(current->kstack + current->kstacksz - sizeof(cpu_context_t));
		hal_cpuSetReturnValue(ctx, 0);
		hal_cpuGuard(ctx, current->kstack);
		hal_longjmp(ctx);
	}
	else {
		hal_jmp(entry, current->kstack + current->kstacksz, stack, 0);
	}
}


static void process_vforkthr(void *arg)
{
	thread_t *current, *parent = (thread_t *)arg;

	current = proc_current();

	hal_spinlockSet(&parent->execwaitsl);
	while (parent->execfl < FORKING)
		proc_threadWait(&parent->execwaitq, &parent->execwaitsl, 0);

	current->execparent = parent;
	hal_spinlockClear(&parent->execwaitsl);

	posix_clone(parent->process->id);

	/* Share parent's resources */
	current->process->resources = parent->process->resources;
	current->process->rlock = parent->process->rlock;

	/* Copy parent kernel stack */
	if ((current->parentkstack = (void *)vm_kmalloc(parent->kstacksz)) == NULL) {
		current->process->mapp = NULL;
		process_exexepilogue(-1, current, parent, NULL, NULL);
	}

	hal_memcpy(current->parentkstack + (hal_cpuGetSP(parent->context) - parent->kstack), hal_cpuGetSP(parent->context), parent->kstack + parent->kstacksz - hal_cpuGetSP(parent->context));

	current->execfl = PARENTSTACK;

	current->execkstack = current->kstack;
	current->kstack = parent->kstack;
	_hal_cpuSetKernelStack(current->kstack + current->kstacksz);

	/* Start execution from parent suspend point */
	hal_longjmp(parent->context);

	/* This part of code left unexecuted */
	return;
}


int proc_vfork(void)
{
	process_t *process, *parent;
	thread_t *current;
	int res = 0;

	if (((current = proc_current()) == NULL) || ((parent = current->process) == NULL))
		return -EINVAL;

	if ((process = (process_t *)vm_kmalloc(sizeof(process_t))) == NULL) {
		return -ENOMEM;
	}

#ifdef NOMMU
	process->lazy = 1;
	process->entries = NULL;
#else
	process->lazy = 0;
#endif

	process->id = -(long)process;
	process->state = NORMAL;
	process->children = NULL;
	process->parent = parent;
	process->threads = NULL;
	process->path = NULL;
	proc_lockInit(&process->lock);

	process->waitq = NULL;
	process->waitpid = 0;

	process->ports = NULL;

	/* Use memory map of parent process until execl or exist are executed */
	process->mapp = parent->mapp;

	process->sigpend = 0;
	process->sigmask = parent->sigmask;
	process->sighandler = parent->sighandler;
	process->zombies = NULL;

	process->ghosts = NULL;
	process->gwaitq = NULL;
	process->waittid = 0;
//	vm_mapCreate(&process->map, (void *)VADDR_MIN, process_common.kmap->start);
//	vm_mapCopy(&parent->mapp, process->mapp);

	proc_lockSet(&process->parent->lock);
	LIST_ADD(&process->parent->children, process);
	proc_lockClear(&process->parent->lock);

	current->execwaitq = NULL;
	current->execfl = PREFORK;

	/* Start first thread */
	if (proc_threadCreate(process, (void *)process_vforkthr, NULL, 4, SIZE_KSTACK, NULL, 0, (void *)current) < 0) {
//		proc_threadDestroy();
		vm_kfree(process);
		return -EINVAL;
	}

	proc_lockSet(&process_common.lock);
	lib_rbInsert(&process_common.id, &process->idlinkage);
	proc_lockClear(&process_common.lock);

	/* Signal forking state to vfork thread */
	hal_spinlockSet(&current->execwaitsl);
	current->execfl = FORKING;
	proc_threadWakeup(&current->execwaitq);

	while (current->execfl < FORKED) {

		/*
		 * proc_threadWait call stores context on the stack - this allows to execute child
		 * thread starting from this point
		 */
		res = proc_threadWait(&current->execwaitq, &current->execwaitsl, 0);

		/* Am I child thread? */
		if (current != proc_current()) {
			res = 1;
			break;
		}
	}

	hal_spinlockClear(&current->execwaitsl);

	if (res == 1)
		return 0;

	return current->execfl == NOFORK ? -ENOMEM : process->id;
}


#ifdef NOMMU

int process_load(process_t *process, thread_t *current, syspage_program_t *prog, const char *path, int argc, char **argv, char **env, void **ustack, void **entry)
{
	unsigned int i, offset;
	void *vaddr, *stack;
	size_t memsz, filesz;
	u32 flags;
	u32 *data = NULL, *dataBase = NULL, *bss = NULL;

	offset = (u32)prog + (u32)prog->offset;
	*entry = (void *)((u32)prog + (u32)prog->entry);

	for (i = 0;; i++) {
		if ((prog == NULL) || (i >= prog->hdrssz))
			break;

		vaddr = (void *)prog->hdrs[i].vaddr;
		memsz = prog->hdrs[i].memsz;
		flags = prog->hdrs[i].flags;
		filesz = prog->hdrs[i].filesz;

		if (flags & 0x4)
				continue;

		if (filesz == memsz) {
			/* .data */
			data = vm_mmap(process->mapp, 0, NULL, (filesz + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1), 0, NULL, -1, flags);
			hal_memcpy(data, offset + vaddr, filesz);
			dataBase = vaddr;
		}
		else if (filesz == 0) {
			/* .bss */
			bss = vm_mmap(process->mapp, 0, NULL, (memsz + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1), 0, NULL, -1, flags);
			hal_memset(bss, 0, memsz);
		}
	}

	/* Perform .got relocation */
	if (data != NULL) {
		for (i = 0; i < (prog->gotsz >> 2); ++i) {
			if (data[i] < (u32)dataBase) {
				data[i] = data[i] + offset;
			}
			else if (data[i] < 0x20000000) {
				data[i] = data[i] - (u32)dataBase + (u32)data;
			}
			else {
				data[i] = data[i] - 0x20000000 + (u32)bss;
			}
		}
	}

	/* Allocate and map user stack */
	stack = vm_mmap(process->mapp, (void *)(VADDR_KERNEL - 2 * SIZE_PAGE), NULL, 2 * SIZE_PAGE, 0, NULL, -1, MAP_NONE);

	pmap_switch(&process->mapp->pmap);
	_hal_cpuSetKernelStack(current->kstack + current->kstacksz);

	/* Copy data from kernel stack */

	stack += 2 * SIZE_PAGE;

	/* Put on stack .got base address. hal_jmp will use it to set r9 */
	PUTONSTACK(stack, void *, data);
	process->got = data;

	*ustack = stack;

	return EOK;
}


int process_exec(syspage_program_t *prog, process_t *process, thread_t *current, thread_t *parent, char *path, int argc, char **argv, char **envp)
{
	void *stack, *entry, *kstack = argv;

	if (parent == NULL) {
		/* Exec into old process, clean up */
		proc_threadsDestroy(process);
		proc_portsDestroy(process);
		vm_mapDestroy(process, process->mapp);
		vm_kfree(current->execkstack);
		current->execkstack = NULL;
		vm_kfree(process->path);
	}

	process->path = path;

	/* Map executable */
	process_load(process, current, prog, path, argc, argv, envp, &stack, &entry);

	resource_init(process);

	if (parent == NULL)
		process_exexepilogue(1, current, parent, entry, stack);

	current->kstack = current->execkstack;

	PUTONSTACK(kstack, void *, stack);
	PUTONSTACK(kstack, void *, entry);
	PUTONSTACK(kstack, thread_t *, parent);
	PUTONSTACK(kstack, thread_t *, current);
	PUTONSTACK(kstack, int, 1);

	hal_jmp(process_exexepilogue, kstack, NULL, 5);

	/* Not reached */
	return 0;
}


#else

int process_load(vm_map_t *map, syspage_program_t *prog, const char *path, int argc, char **argv, char **env, void **ustack, void **entry)
{
	oid_t oid;
	vm_object_t *o = NULL;
	void *vaddr = NULL, *stack;
	size_t memsz = 0, filesz = 0, osize;
	offs_t offs = 0, base;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	unsigned prot, flags, misalign = 0;
	int i, envc;

	pmap_switch(&map->pmap);

	if (prog == NULL) {
		if (proc_lookup(path, NULL, &oid) < 0)
			return -ENOENT;

		if (vm_objectGet(&o, oid) < 0)
			return -ENOMEM;

		osize = round_page(o->size);
		base = 0;
	}
	else {
		o = (void *)-1;
		osize = round_page(prog->end - prog->start);
		base = prog->start;
	}

	if ((ehdr = vm_mmap(process_common.kmap, NULL, NULL, osize, PROT_READ, o, base, MAP_NONE)) == NULL) {
		vm_objectPut(o);
		return -ENOMEM;
	}

	/* Test ELF header */
	if (hal_strncmp((char *)ehdr->e_ident, "\177ELF", 4)) {
		vm_munmap(process_common.kmap, ehdr, osize);
		vm_objectPut(o);
		return -ENOEXEC;
	}

	*entry = (void *)(unsigned long)ehdr->e_entry;

	for (i = 0, phdr = (void *)ehdr + ehdr->e_phoff; i < ehdr->e_phnum; i++, phdr++) {
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

		if (filesz && vm_mmap(map, vaddr, NULL, round_page(filesz), prot, o, base + offs, flags) == NULL) {
			vm_munmap(process_common.kmap, ehdr, osize);
			vm_objectPut(o);
			return -ENOMEM;
		}

		if (filesz != memsz) {
			if (round_page(memsz) - round_page(filesz) && vm_mmap(map, vaddr, NULL, round_page(memsz) - round_page(filesz), prot, NULL, -1, MAP_NONE) == NULL) {
				vm_munmap(process_common.kmap, ehdr, osize);
				vm_objectPut(o);
				return -ENOMEM;
			}

			hal_memset(vaddr + filesz, 0, round_page((unsigned long)vaddr + memsz) - (unsigned long)vaddr - filesz);
		}
	}

	vm_munmap(process_common.kmap, ehdr, osize);
	vm_objectPut(o);

	/* Allocate and map user stack */
	if ((stack = vm_mmap(map, map->pmap.end - 8 * SIZE_PAGE, NULL, 8 * SIZE_PAGE, PROT_READ | PROT_WRITE | PROT_USER, NULL, -1, MAP_NONE)) == NULL)
		return -ENOMEM;

	stack += 8 * SIZE_PAGE;

	/* Copy data from kernel stack */
	for (i = 0; i < argc; ++i) {
		memsz = hal_strlen(argv[i]) + 1;
		hal_memcpy(stack -= memsz, argv[i], memsz);
		argv[i] = stack;
	}

	stack -= (unsigned long)stack & (sizeof(int) - 1);

	PUTONSTACK(stack, void *, NULL); /* argv sentinel */
	stack -= argc * sizeof(char *);

	hal_memcpy(stack, argv, argc * sizeof(char *));
	argv = stack;

	/* Copy env from kernel stack */
	if (env) {
		for (envc = 0; env[envc] != NULL; ++envc) {
			memsz = hal_strlen(env[envc]) + 1;
			hal_memcpy(stack -= memsz, env[envc], memsz);
			env[envc] = stack;
		}

		stack -= (unsigned long)stack & (sizeof(int) - 1);

		PUTONSTACK(stack, void *, NULL); /* env sentinel */
		stack -= envc * sizeof(char *);

		hal_memcpy(stack, env, envc * sizeof(char *));
		env = stack;
	}

	PUTONSTACK(stack, char **, env);
	PUTONSTACK(stack, char **, argv);
	PUTONSTACK(stack, int, argc);
	PUTONSTACK(stack, void *, NULL); /* return address */

	*ustack = stack;

	return EOK;
}


int process_exec(syspage_program_t *prog, process_t *process, thread_t *current, thread_t *parent, char *path, int argc, char **argv, char **envp)
{
	vm_map_t map, *mapp;
	page_t *p;
	void *v, *stack, *entry, *kstack = argv;
	int i = 0, err;
	addr_t a;

	/* Create map and pmap for new process, keep old in case of failure */
	mapp = process->mapp;

	if ((p = vm_pageAlloc(SIZE_PDIR, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE)) == NULL)
		return -ENOMEM;

	if ((v = vm_mmap(process_common.kmap, process_common.kmap->start, p, 1 << p->idx, PROT_READ | PROT_WRITE, process_common.kernel, -1, MAP_NONE)) == NULL) {
		vm_pageFree(p);
		return -ENOMEM;
	}

	vm_mapCreate(&map, (void *)VADDR_MIN + SIZE_PAGE, (void *)VADDR_USR_MAX);
	pmap_create(&map.pmap, &process_common.kmap->pmap, p, v);
	process->mapp = &map;

	/* Map executable */
	if ((err = process_load(&map, prog, path, argc, argv, envp, &stack, &entry)) < 0) {
		process->mapp = mapp;
		pmap_switch(&mapp->pmap);

		vm_mapDestroy(process, &map);
		while ((a = pmap_destroy(&map.pmap, &i)))
			vm_pageFree(_page_get(a));
		vm_munmap(process_common.kmap, v, SIZE_PDIR);
		vm_pageFree(p);
		return err;
	}

	if (parent == NULL) {
		/* Exec into old process, clean up */
		proc_threadsDestroy(process);
		proc_portsDestroy(process);
		proc_resourcesFree(process);
		vm_mapDestroy(process, &process->map);
		while ((a = pmap_destroy(&process->map.pmap, &i)))
			vm_pageFree(_page_get(a));
		vm_munmap(process_common.kmap, process->pmapv, SIZE_PDIR);
		vm_pageFree(process->pmapp);
		vm_kfree(current->execkstack);
		current->execkstack = NULL;
		vm_kfree(process->path);
	}

	resource_init(process);

	vm_mapMove(&process->map, &map);
	process->mapp = &process->map;
	process->path = path;
	process->pmapv = v;
	process->pmapp = p;

	if (parent == NULL)
		process_exexepilogue(1, current, parent, entry, stack);

	current->kstack = current->execkstack;

	PUTONSTACK(kstack, void *, stack);
	PUTONSTACK(kstack, void *, entry);
	PUTONSTACK(kstack, thread_t *, parent);
	PUTONSTACK(kstack, thread_t *, current);
	PUTONSTACK(kstack, int, 1);

	hal_jmp(process_exexepilogue, kstack, NULL, 5);

	/* Not reached */
	return 0;
}

#endif


#ifndef NOMMU

int proc_copyexec(void)
{
	process_t *process;
	thread_t *parent, *current = proc_current();
	void *kstack;
	int len;

	process = current->process;
	parent = current->execparent;

	current->kstack = current->execkstack;
	kstack = current->execkstack + current->kstacksz - sizeof(cpu_context_t);
	hal_memcpy(kstack, current->parentkstack + current->kstacksz - sizeof(cpu_context_t), sizeof(cpu_context_t));

	PUTONSTACK(kstack, void *, NULL);
	PUTONSTACK(kstack, void *, NULL);
	PUTONSTACK(kstack, thread_t *, parent);
	PUTONSTACK(kstack, thread_t *, current);

	len = hal_strlen(parent->process->path) + 1;
	if ((process->path = vm_kmalloc(len)) == NULL) {
		PUTONSTACK(kstack, int, -1); /* exec */
		hal_jmp(process_exexepilogue, kstack, NULL, 5);
	}
	hal_memcpy(process->path, parent->process->path, len);

	if ((process->pmapp = vm_pageAlloc(SIZE_PDIR, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE)) == NULL) {
		PUTONSTACK(kstack, int, -1); /* exec */
		hal_jmp(process_exexepilogue, kstack, NULL, 5);
	}

	if ((process->pmapv = vm_mmap(process_common.kmap, process_common.kmap->start, process->pmapp, 1 << process->pmapp->idx, PROT_READ | PROT_WRITE, process_common.kernel, -1, MAP_NONE)) == NULL) {
		vm_pageFree(process->pmapp);
		PUTONSTACK(kstack, int, -1); /* exec */
		hal_jmp(process_exexepilogue, kstack, NULL, 5);
	}

	pmap_create(&process->map.pmap, &process_common.kmap->pmap, process->pmapp, process->pmapv);
	vm_mapCreate(&process->map, parent->process->mapp->start, parent->process->mapp->stop);
	pmap_switch(&process->map.pmap);
	process->mapp = &process->map;

	/* Initialize resources tree for mutex, cond and file handles */
	resource_init(current->process);

	if (proc_resourcesCopy(parent->process) < 0) {
		current->process->mapp = NULL;
		PUTONSTACK(kstack, int, -1); /* exec */
	}
	else if (vm_mapCopy(process, &process->map, &parent->process->map) < 0) {
		PUTONSTACK(kstack, int, -1); /* exec */
	}
	else {
		PUTONSTACK(kstack, int, 2); /* exec */
	}

	hal_jmp(process_exexepilogue, kstack, NULL, 5);

	/* Not reached */
	return EOK;
}

#else

int proc_copyexec(void)
{
	return -EINVAL;
}

#endif


int proc_execve(syspage_program_t *prog, const char *path, char **argv, char **envp)
{
	int len, argc, envc, err;
	void *kstack;
	thread_t *current, *parent;
	process_t *process;
	char *kpath;
	char **envp_kstack = NULL;
	char **argv_kstack;

	current = proc_current();
	parent = current->execparent;
	process = current->process;

	len = hal_strlen(path) + 1;

	if ((kpath = vm_kmalloc(len)) == NULL)
		return -ENOMEM;

	hal_memcpy(kpath, path, len);

	if (parent == NULL && (current->execkstack = vm_kmalloc(current->kstacksz)) == NULL) {
		vm_kfree(kpath);
		return -ENOMEM;
	}

	kstack = current->execkstack + current->kstacksz;

	for (argc = 0; argv[argc] != NULL; ++argc)
		;

	kstack -= argc * sizeof(char *);
	argv_kstack = kstack;

	for (argc = 0; argv[argc] != NULL; ++argc) {
		len = hal_strlen(argv[argc]) + 1;
		kstack -= (len + sizeof(int) - 1) & ~(sizeof(int) - 1);
		hal_memcpy(kstack, argv[argc], len);
		argv_kstack[argc] = kstack;
	}

	if (envp) {
		for (envc = 0; envp[envc] != NULL; ++envc)
			;

		kstack -= (envc + 1) * sizeof(char *);
		envp_kstack = kstack;
		envp_kstack[envc] = NULL;

		for (envc = 0; envp[envc] != NULL; ++envc) {
			len = hal_strlen(envp[envc]) + 1;
			kstack -= (len + sizeof(int) - 1) & ~(sizeof(int) - 1);
			hal_memcpy(kstack, envp[envc], len);
			envp_kstack[envc] = kstack;
		}
	}

	/* Close cloexec file descriptors */
	posix_exec();

	err = process_exec(prog, process, current, parent, kpath, argc, argv_kstack, envp_kstack);
	/* Not reached unless process_exec failed */

	vm_kfree(kpath);
	return err;
}


int proc_execle(syspage_program_t *prog, const char *path, ...)
{
	va_list ap;
	thread_t *current, *parent;
	process_t *process;
	const char *s;
	void *kstack, *args, *envp = NULL;
	char **argv, *kpath;
	int argc, i, err, len;

	current = proc_current();
	parent = current->execparent;
	process = current->process;

	len = hal_strlen(path) + 1;

	if ((kpath = vm_kmalloc(len)) == NULL)
		return -ENOMEM;

	hal_memcpy(kpath, path, len);

	if (parent == NULL && (current->execkstack = vm_kmalloc(current->kstacksz)) == NULL) {
		vm_kfree(kpath);
		return -ENOMEM;
	}

	kstack = current->execkstack + current->kstacksz;

	/* Calculate args size */
	va_start(ap, path);
	for (argc = 0;; argc++) {
		if ((s = va_arg(ap, char *)) == NULL)
			break;
		kstack -= ((hal_strlen(s) + 1 + sizeof(int) - 1) & ~(sizeof(int) - 1));
	}
	va_end(ap);

	/* Allocate argv */
	args = kstack;
	argv = kstack - argc * sizeof(char *);
	kstack = argv;

	/* Copy args to kernel stack */
	va_start(ap, path);
	for (i = 0; i < argc; i++) {
		s = va_arg(ap, char *);
		len = hal_strlen(s) + 1;
		hal_memcpy(args, s, len);
		argv[i] = args;
		args += ((len + sizeof(int) - 1) & ~(sizeof(int) - 1));
	}
	va_end(ap);

	/* Close cloexec file descriptors */
	posix_exec();

	/* Calculate env[] size */

	/* Copy env[] to kernel stack */

	err = process_exec(prog, process, current, parent, kpath, argc, argv, envp);
	/* Not reached unless process_exec failed */

	vm_kfree(kpath);
	return err;
}


void proc_exit(int code)
{
	thread_t *current, *parent;
	void *kstack;

	current = proc_current();
	parent = current->execparent;

	if (current->process != NULL)
		current->process->exit = code;

	if (current->parentkstack != NULL) {
		current->kstack = current->execkstack;
		current->process->mapp = NULL;

		kstack = current->kstack + current->kstacksz;

		PUTONSTACK(kstack, void *, NULL);
		PUTONSTACK(kstack, void *, NULL);
		PUTONSTACK(kstack, thread_t *, parent);
		PUTONSTACK(kstack, thread_t *, current);
		PUTONSTACK(kstack, int, 0);

		hal_jmp(process_exexepilogue, kstack, NULL, 5);
	}

	process_exexepilogue(0, current, parent, NULL, NULL);

	/* Not reached */
}


int _process_init(vm_map_t *kmap, vm_object_t *kernel)
{
	process_common.kmap = kmap;
	process_common.first = NULL;
	process_common.kernel = kernel;
	proc_lockInit(&process_common.lock);
	lib_rbInit(&process_common.id, proc_idcmp, NULL);
	hal_exceptionsSetHandler(EXC_DEFAULT, process_exception);
	hal_exceptionsSetHandler(EXC_UNDEFINED, process_illegal);
	return EOK;
}
