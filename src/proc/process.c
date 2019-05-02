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


process_t *proc_find(unsigned int pid)
{
	process_t *p, s;
	s.id = pid;

	proc_lockSet(&process_common.lock);
	p = lib_treeof(process_t, idlinkage, lib_rbFind(&process_common.id, &s.idlinkage));
	proc_lockClear(&process_common.lock);

	return p;
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

	process->state = NORMAL;
	process->path = NULL;

	if (path != NULL) {
		if ((process->path = vm_kmalloc(hal_strlen(path) + 1)) == NULL) {
			vm_kfree(process);
			return -ENOMEM;
		}

		hal_strcpy(process->path, path);
	}

	process->argv = NULL;
	process->parent = NULL;
	process->children = NULL;
	process->threads = NULL;

	process->waitq = NULL;
	process->waitpid = 0;

	proc_lockInit(&process->lock);

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
#else
	process->lazy = 1;
#endif

	process->mapp = process_common.kmap;

	/* Initialize resources tree for mutex and cond handles */
	resource_init(process);
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
	process_t *child, *zombie, *init;

	perf_kill(proc);
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
	current->process->parent = parent->process;
	current->process->mapp = parent->process->mapp;
	current->process->sigmask = parent->process->sigmask;
	current->process->sighandler = parent->process->sighandler;
	hal_cpuReschedule(&parent->execwaitsl);

	proc_lockSet(&parent->process->lock);
	LIST_ADD(&parent->process->children, current->process);
	proc_lockClear(&parent->process->lock);

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
	thread_t *current;
	int pid, res = 0;

	if ((current = proc_current()) == NULL)
		return -EINVAL;

	current->execwaitq = NULL;
	current->execfl = PREFORK;

	if ((pid = proc_start(process_vforkthr, current, NULL)) < 0)
		return pid;

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

	return current->execfl == NOFORK ? -ENOMEM : pid;
}


#ifdef NOMMU
struct _reloc {
	void *vbase;
	void *pbase;
	size_t size;
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


int process_load(process_t *process, syspage_program_t *prog, const char *path, int argc, char **argv, char **env, void **ustack, void **entry)
{
	void *vaddr = NULL, *stack, *paddr, *base;
	size_t memsz = 0, filesz = 0, osize;
	offs_t offs = 0;
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	Elf32_Shdr *shdr;
	unsigned prot, flags, misalign = 0;
	int i, j, relocsz = 0;
	char **uargv, *snameTab;
	ptr_t *got;
	struct _reloc reloc[4];

	if (prog == NULL)
		return -ENOEXEC;

	osize = round_page(prog->end - prog->start);
	base = (void *)prog->start;

	ehdr = base;

	/* Test ELF header */
	if (hal_strncmp((char *)ehdr->e_ident, "\177ELF", 4) || ehdr->e_shnum == 0)
		return -ENOEXEC;

	hal_memset(reloc, 0, sizeof(reloc));

	for (i = 0, j = 0, phdr = (void *)ehdr + ehdr->e_phoff; i < ehdr->e_phnum; i++, phdr++) {
		if (phdr->p_type != PT_LOAD)
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

		if (phdr->p_flags & PF_X)
			prot |= PROT_EXEC;

		if (phdr->p_flags & PF_W) {
			prot |= PROT_WRITE;

			if ((paddr = vm_mmap(process->mapp, NULL, NULL, round_page(memsz), prot, NULL, -1, flags)) == NULL)
				return -ENOMEM;

			if (filesz) {
				if (offs + round_page(filesz) > osize)
					return -ENOEXEC;

				hal_memcpy(paddr, (char *)ehdr + offs, filesz);
			}

			hal_memset((char *)paddr + filesz, 0, round_page(memsz) - filesz);
		}
		else {
			paddr = (char *)ehdr + offs;
		}

		if (j > sizeof(reloc) / sizeof(reloc[0]))
			return -ENOMEM;

		reloc[j].vbase = vaddr;
		reloc[j].pbase = paddr;
		reloc[j].size = memsz;
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

	/* Perform relocations */
	for (i = 0; i < shdr->sh_size / 4; ++i) {
		if (process_relocate(reloc, relocsz, (char **)&got[i]) < 0)
			return -ENOEXEC;
	}

	*entry = (void *)(unsigned long)ehdr->e_entry;
	if (process_relocate(reloc, relocsz, (char **)entry) < 0)
		return -ENOEXEC;

	/* Allocate and map user stack */
	if ((stack = vm_mmap(process->mapp, NULL, NULL, SIZE_USTACK, PROT_READ | PROT_WRITE | PROT_USER, NULL, -1, MAP_NONE)) == NULL)
		return -ENOMEM;

	stack += SIZE_USTACK;

	stack -= (argc + 1) * sizeof(char *);
	uargv = stack;

	/* Copy data from kernel stack */
	for (i = 0; i < argc; ++i) {
		memsz = hal_strlen(argv[i]) + 1;
		hal_memcpy(stack -= memsz, argv[i], memsz);
		uargv[i] = stack;
	}

	uargv[i] = NULL;

	stack -= (unsigned long)stack & (sizeof(int) - 1);

	PUTONSTACK(stack, char **, NULL); /* env */
	PUTONSTACK(stack, char **, uargv);
	PUTONSTACK(stack, int, argc);
	PUTONSTACK(stack, void *, NULL); /* return address */

	/* Put on stack .got base address. hal_jmp will use it to set r9 */
	PUTONSTACK(stack, void *, got);
	process->got = (void *)got;

	*ustack = stack;

	return EOK;
}


int process_exec(syspage_program_t *prog, process_t *process, thread_t *current, thread_t *parent, char *path, int argc, char **argv, char **envp, void *kstack)
{
	void *stack, *entry;
	int err;

	/* Map executable */
	if ((err = process_load(process, prog, path, argc, argv, envp, &stack, &entry)) < 0)
		return err;

	if (parent == NULL) {
		/* Exec into old process, clean up */
		proc_threadsDestroy(process);
		proc_portsDestroy(process);
		vm_mapDestroy(process, process->mapp);
		vm_kfree(current->execkstack);
		current->execkstack = NULL;
		vm_kfree(process->path);
		if (process->argv != NULL)
			vm_kfree(process->argv);
	}

	process->path = path;

	process->sigpend = 0;
	process->sigmask = 0;
	process->sighandler = NULL;
	process->path = path;
	process->argv = argv;

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

int process_load(process_t *process, syspage_program_t *prog, const char *path, int argc, char **argv, char **env, void **ustack, void **entry)
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
	char **uargv;

	pmap_switch(&process->mapp->pmap);

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

		if (filesz && vm_mmap(process->mapp, vaddr, NULL, round_page(filesz), prot, o, base + offs, flags) == NULL) {
			vm_munmap(process_common.kmap, ehdr, osize);
			vm_objectPut(o);
			return -ENOMEM;
		}

		if (filesz != memsz) {
			if (round_page(memsz) - round_page(filesz) && vm_mmap(process->mapp, vaddr, NULL, round_page(memsz) - round_page(filesz), prot, NULL, -1, MAP_NONE) == NULL) {
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
	if ((stack = vm_mmap(process->mapp, process->mapp->pmap.end - SIZE_USTACK, NULL, SIZE_USTACK, PROT_READ | PROT_WRITE | PROT_USER, NULL, -1, MAP_NONE)) == NULL)
		return -ENOMEM;

	stack += SIZE_USTACK;

	stack -= (argc + 1) * sizeof(char *);
	uargv = stack;

	/* Copy data from kernel stack */
	for (i = 0; i < argc; ++i) {
		memsz = hal_strlen(argv[i]) + 1;
		hal_memcpy(stack -= memsz, argv[i], memsz);
		uargv[i] = stack;
	}

	uargv[i] = NULL;

	stack -= (unsigned long)stack & (sizeof(int) - 1);

	/* Copy env from kernel stack */
	if (env != NULL) {
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
	PUTONSTACK(stack, char **, uargv);
	PUTONSTACK(stack, int, argc);
	PUTONSTACK(stack, void *, NULL); /* return address */

	*ustack = stack;

	return EOK;
}


int process_exec(syspage_program_t *prog, process_t *process, thread_t *current, thread_t *parent, char *path, int argc, char **argv, char **envp, void *kstack)
{
	vm_map_t map, *mapp;
	void *stack, *entry;
	int err;

	/* Create map and pmap for new process, keep old in case of failure */
	mapp = process->mapp;
	vm_mapCreate(&map, (void *)VADDR_MIN + SIZE_PAGE, (void *)VADDR_USR_MAX);
	process->mapp = &map;

	/* Map executable */
	if ((err = process_load(process, prog, path, argc, argv, envp, &stack, &entry)) < 0) {
		process->mapp = mapp;
		pmap_switch(&mapp->pmap);
		vm_mapDestroy(process, &map);
		return err;
	}

	if (parent == NULL) {
		/* Exec into old process, clean up */
		proc_threadsDestroy(process);
		proc_portsDestroy(process);
		proc_resourcesFree(process);
		vm_mapDestroy(process, &process->map);
		vm_kfree(current->execkstack);
		current->execkstack = NULL;
		vm_kfree(process->path);
		if (process->argv != NULL)
			vm_kfree(process->argv);
	}

	resource_init(process);

	process->sigpend = 0;
	process->sigmask = 0;
	process->sighandler = NULL;

	vm_mapMove(&process->map, &map);
	process->mapp = &process->map;
	process->path = path;
	process->argv = argv;

	perf_exec(process, path);

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
	hal_memcpy(kstack, parent->kstack + current->kstacksz - sizeof(cpu_context_t), sizeof(cpu_context_t));

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
	int len, argc = 0, envc, err;
	void *kstack;
	thread_t *current, *parent;
	process_t *process;
	char *kpath;
	char **kargv = NULL, *buf;
	char **envp_kstack = NULL;


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

	len = 0;
	if (argv != NULL) {
		for (argc = 0; argv[argc] != NULL; ++argc)
			len += hal_strlen(argv[argc]) + 1;
	}

	if (argc) {
		if ((kargv = vm_kmalloc(len + (argc + 1) * sizeof(char *))) == NULL) {
			vm_kfree(kpath);
			vm_kfree(current->execkstack);
			return -ENOMEM;
		}

		buf = (char *)(kargv + argc + 1);

		for (argc = 0; argv[argc] != NULL; ++argc) {
			len = hal_strlen(argv[argc]) + 1;
			hal_memcpy(buf, argv[argc], len);
			kargv[argc] = buf;
			buf += len;
		}

		kargv[argc] = NULL;
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

	err = process_exec(prog, process, current, parent, kpath, argc, kargv, envp_kstack, kstack);
	/* Not reached unless process_exec failed */

	vm_kfree(kpath);
	if (kargv != NULL)
		vm_kfree(kargv);
	return err;
}


void proc_exit(int code)
{
	thread_t *current, *parent;
	void *kstack;

	current = proc_current();
	parent = current->execparent;

	if (current->process != NULL)
		current->process->exit = code & 0xff;

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
	process_common.idcounter = 1;
	proc_lockInit(&process_common.lock);
	lib_rbInit(&process_common.id, proc_idcmp, process_augment);
	hal_exceptionsSetHandler(EXC_DEFAULT, process_exception);
	hal_exceptionsSetHandler(EXC_UNDEFINED, process_illegal);
	return EOK;
}
