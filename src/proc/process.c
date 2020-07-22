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


typedef struct {
	spinlock_t sl;
	thread_t *wq;
	volatile int state;
	thread_t *parent;

	vm_object_t *object;
	offs_t offset;
	size_t size;
	vm_map_t *map;

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

	perf_kill(p);

	posix_died(p->id, p->exit);

	if (p->mapp != NULL)
		vm_mapDestroy(p, p->mapp);

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

	if ((process = vm_kmalloc(sizeof(process_t))) == NULL)
		return -ENOMEM;

#ifdef NOMMU
	process->entries = NULL;
#endif

	process->path = NULL;

	if (path != NULL) {
		if ((process->path = vm_kmalloc(hal_strlen(path) + 1)) == NULL) {
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

	proc_lockInit(&process->lock);

	process->ports = NULL;

	process->sigpend = 0;
	process->sigmask = 0;
	process->sighandler = NULL;

#ifndef NOMMU
	process->lazy = 0;
#else
	process->lazy = 1;
#endif

	process->mapp = NULL;
	process->pmapp = NULL;

	/* Initialize resources tree for mutex and cond handles */
	_resource_init(process);
	process_alloc(process);
	perf_fork(process);

	if (proc_threadCreate(process, (void *)initthr, NULL, 4, SIZE_KSTACK, NULL, 0, (void *)arg) < 0) {
		vm_kfree(process);
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
	thread_t *thread = proc_current();
	process_t *process = thread->process;
	userintr_t *intr;
	char buff[SIZE_CTXDUMP];

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);
	hal_consolePrint(ATTR_BOLD, "\n");

	if ((intr = userintr_active()) != NULL)
		lib_printf("in interrupt (%d) handler of process \"%s\" (%x)\n", intr->handler.n, intr->process->path, intr->process->id);
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


#ifndef NOMMU

int process_load(process_t *process, vm_object_t *o, offs_t base, size_t size, void **ustack, void **entry)
{
	void *vaddr = NULL, *stack;
	size_t memsz = 0, filesz = 0;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	unsigned i, prot, flags, misalign = 0;
	offs_t offs;
	vm_map_t *map = process->mapp;

	size = round_page(size);

	if ((ehdr = vm_mmap(process_common.kmap, NULL, NULL, size, PROT_READ, o, base, MAP_NONE)) == NULL)
		return -ENOMEM;

	/* Test ELF header */
	if (hal_strncmp((char *)ehdr->e_ident, "\177ELF", 4)) {
		vm_munmap(process_common.kmap, ehdr, size);
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
			vm_munmap(process_common.kmap, ehdr, size);
			return -ENOMEM;
		}

		if (filesz != memsz) {
			if (round_page(memsz) - round_page(filesz) && vm_mmap(map, vaddr, NULL, round_page(memsz) - round_page(filesz), prot, NULL, -1, MAP_NONE) == NULL) {
				vm_munmap(process_common.kmap, ehdr, size);
				return -ENOMEM;
			}

			hal_memset(vaddr + filesz, 0, round_page((unsigned long)vaddr + memsz) - (unsigned long)vaddr - filesz);
		}
	}

	vm_munmap(process_common.kmap, ehdr, size);

	/* Allocate and map user stack */
	if ((stack = vm_mmap(map, map->pmap.end - SIZE_USTACK, NULL, SIZE_USTACK, PROT_READ | PROT_WRITE | PROT_USER, NULL, -1, MAP_NONE)) == NULL)
		return -ENOMEM;

	*ustack = stack + SIZE_USTACK;

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
	Elf32_Rel *rel;
	unsigned prot, flags;
	int i, j, relocsz = 0, reltype;
	void *relptr;
	char *snameTab;
	ptr_t *got;
	struct _reloc reloc[5];

	if (o != (void *)-1)
		return -ENOEXEC;

	ehdr = (void *)(ptr_t)base;

	/* Test ELF header */
	if (hal_strncmp((char *)ehdr->e_ident, "\177ELF", 4) || ehdr->e_shnum == 0)
		return -ENOEXEC;

	hal_memset(reloc, 0, sizeof(reloc));

	for (i = 0, j = 0, phdr = (void *)ehdr + ehdr->e_phoff; i < ehdr->e_phnum; i++, phdr++) {
		if (phdr->p_type != PT_LOAD)
			continue;

		prot = PROT_USER;
		flags = MAP_NONE;

		if (phdr->p_flags & PF_R)
			prot |= PROT_READ;

		if (phdr->p_flags & PF_X)
			prot |= PROT_EXEC;

		if (phdr->p_flags & PF_W) {
			prot |= PROT_WRITE;

			if ((paddr = vm_mmap(process->mapp, NULL, NULL, round_page(phdr->p_memsz), prot, NULL, -1, flags)) == NULL)
				return -ENOMEM;

			if (phdr->p_filesz) {
				if (phdr->p_offset + round_page(phdr->p_filesz) > size)
					return -ENOEXEC;

				hal_memcpy(paddr, (char *)ehdr + phdr->p_offset, phdr->p_filesz);
			}

			hal_memset((char *)paddr + phdr->p_filesz, 0, round_page(phdr->p_memsz) - phdr->p_filesz);
		}
		else {
			paddr = (char *)ehdr + phdr->p_offset;
		}

		if (j > sizeof(reloc) / sizeof(reloc[0]))
			return -ENOMEM;

		reloc[j].vbase = (void *)phdr->p_vaddr;
		reloc[j].pbase = paddr;
		reloc[j].size = phdr->p_memsz;
		reloc[j].misalign = phdr->p_offset & (phdr->p_align - 1);
		++relocsz;
		++j;
	}

	/* Find .got section */
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

	/* Perform data relocation */
	for (i = 0, shdr = (void *)((char *)ehdr + ehdr->e_shoff); i < ehdr->e_shnum; i++, shdr++) {
		if (hal_strncmp(&snameTab[shdr->sh_name], ".rel", 4) != 0)
			continue;

		if (!shdr->sh_size || !shdr->sh_entsize)
			continue;

		for (j = 0; j < shdr->sh_size / shdr->sh_entsize; ++j) {
			rel = (void *)((ptr_t)shdr->sh_offset + (ptr_t)ehdr + (j * shdr->sh_entsize));
			reltype = ELF32_R_TYPE(rel->r_info);

			if (reltype == R_ARM_ABS32) {
				relptr = (void *)rel->r_offset;

				if (process_relocate(reloc, relocsz, (char **)&relptr) < 0)
					return -ENOEXEC;

				if (process_relocate(reloc, relocsz, relptr) < 0)
					return -ENOEXEC;
			}
		}
	}

	/* Allocate and map user stack */
	if ((stack = vm_mmap(process->mapp, NULL, NULL, SIZE_USTACK, PROT_READ | PROT_WRITE | PROT_USER, NULL, -1, MAP_NONE)) == NULL)
		return -ENOMEM;

	process->got = (void *)got;
	*ustack = stack + SIZE_USTACK;

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
		stack -= (len + sizeof(int) - 1) & ~(sizeof(int) - 1);
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
	current->process->mapp = &current->process->map;
	current->process->pmapp = &current->process->mapp->pmap;
#else
	current->process->mapp = (spawn->map != NULL) ? spawn->map : process_common.kmap;
	current->process->pmapp = &current->process->map.pmap;
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
		spawn->state = err == EOK ? FORKED : err;
		proc_threadWakeup(&spawn->wq);
		hal_spinlockClear(&spawn->sl, &sc);
	}

	_hal_cpuSetKernelStack(current->kstack + current->kstacksz);
	hal_cpuSetGot(current->process->got);

	if (err < 0)
		proc_threadEnd();
	else
		hal_jmp(entry, current->kstack + current->kstacksz, stack, 0);
}


static void proc_spawnThread(void *arg)
{
	thread_t *current = proc_current();
	process_spawn_t *spawn = arg;

	/* temporary: create new posix process */
	if (spawn->parent != NULL)
		posix_clone(spawn->parent->process->id);

	process_exec(current, spawn);
}


int proc_spawn(vm_object_t *object, vm_map_t *map, offs_t offset, size_t size, const char *path, char **argv, char **envp)
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

	return proc_spawn(object, NULL, 0, object->size, path, argv, envp);
}


int proc_syspageSpawn(syspage_program_t *program, vm_map_t *map, const char *path, char **argv)
{
	return proc_spawn((void *)-1, map, program->start, program->end - program->start, path, argv, NULL);
}


/* (v)fork/exec/exit */


static void proc_vforkedExit(thread_t *current, process_spawn_t *spawn, int state)
{
	thread_t *parent = spawn->parent;
	spinlock_ctx_t sc;

	hal_memcpy(hal_cpuGetSP(parent->context), current->parentkstack + (hal_cpuGetSP(parent->context) - parent->kstack), parent->kstack + parent->kstacksz - hal_cpuGetSP(parent->context));
	vm_kfree(current->parentkstack);

	current->process->mapp = NULL;
	current->process->pmapp = NULL;

	hal_spinlockSet(&spawn->sl, &sc);
	spawn->state = state;
	proc_threadWakeup(&spawn->wq);
	hal_spinlockClear(&spawn->sl, &sc);

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

	current->process->mapp = parent->process->mapp;
	current->process->pmapp = parent->process->pmapp;
	current->process->sigmask = parent->process->sigmask;
	current->process->sighandler = parent->process->sighandler;
	pmap_switch(current->process->pmapp);

	hal_spinlockSet(&spawn->sl, &sc);
	while (spawn->state < FORKING)
		proc_threadWait(&spawn->wq, &spawn->sl, 0, &sc);
	hal_spinlockClear(&spawn->sl, &sc);

	/* Copy parent kernel stack */
	if ((current->parentkstack = (void *)vm_kmalloc(parent->kstacksz)) == NULL) {
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

	_hal_cpuSetKernelStack(current->kstack + current->kstacksz);

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

	if ((current = proc_current()) == NULL)
		return -EINVAL;

	if ((spawn = vm_kmalloc(sizeof(*spawn))) == NULL)
		return -ENOMEM;

	hal_spinlockCreate(&spawn->sl, "execsl");

	spawn->object = NULL;
	spawn->offset = 0;
	spawn->size = 0;
	spawn->wq = NULL;
	spawn->state = PREFORK;
	spawn->parent = current;

	if ((pid = proc_start(process_vforkThread, spawn, NULL)) < 0) {
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
	}
	while (spawn->state < FORKED && spawn->state > 0 && isparent);

	hal_spinlockClear(&spawn->sl, &sc);

	if (isparent) {
		hal_spinlockDestroy(&spawn->sl);
		vm_objectPut(spawn->object);
		ret = spawn->state;
		vm_kfree(spawn);
		return ret < 0 ? ret : pid;
	}

	return 0;
}


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

	process->mapp = &process->map;
	process->pmapp = &process->map.pmap;
	pmap_switch(process->pmapp);
	return EOK;
}


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
	thread_t *current;
	int err;
	void *kstack;
	unsigned sigmask;

	if (!(err = proc_vfork())) {
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

	return err;
}


static int process_execve(thread_t *current)
{
	process_spawn_t *spawn = current->execdata;
	thread_t *parent = spawn->parent;
	vm_map_t *map;

	/* Restore kernel stack of parent thread */
	if (parent != NULL) {
		hal_memcpy(hal_cpuGetSP(parent->context), current->parentkstack + (hal_cpuGetSP(parent->context) - parent->kstack), parent->kstack + parent->kstacksz - hal_cpuGetSP(parent->context));
		vm_kfree(current->parentkstack);
	}
	else {
		/* Reinitialize process */
		map = current->process->mapp;
		current->process->mapp = NULL;
		current->process->pmapp = NULL;
		pmap_switch(&process_common.kmap->pmap);

		vm_mapDestroy(current->process, map);
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
	proc_lockInit(&process_common.lock);
	lib_rbInit(&process_common.id, proc_idcmp, process_augment);
	hal_exceptionsSetHandler(EXC_DEFAULT, process_exception);
	hal_exceptionsSetHandler(EXC_UNDEFINED, process_illegal);
	return EOK;
}
