/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System calls
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

/* parasoft-begin-suppress MISRAC2012-RULE_8_4-a "Compatible function declaration is not possible for syscalls" */

#include "hal/hal.h"
#include "hal/cpu.h"
#include "include/errno.h"
#include "include/sysinfo.h"
#include "include/mman.h"
#include "include/syscalls.h"
#include "include/threads.h"
#include "include/utsname.h"
#include "include/time.h"
#include "lib/lib.h"
#include "proc/proc.h"
#include "vm/object.h"
#include "posix/posix.h"
#include "syspage.h"

#define SYSCALLS_NAME(name)   syscalls_##name,
#define SYSCALLS_STRING(name) #name,

/*
 * Kernel
 */


void syscalls_debug(u8 *ustack)
{
	const char *s;

	/* FIXME: pass strlen(s) from userspace */

	GETFROMSTACK(ustack, const char *, s, 0);
	hal_consolePrint(ATTR_USER, s);
}


/*
 * Memory management
 */


int syscalls_sys_mmap(u8 *ustack)
{
	void **vaddr;
	size_t size;
	int prot, fildes, sflags;
	unsigned int flags;
	off_t offs;
	vm_object_t *o;
	oid_t oid;
	process_t *proc = proc_current()->process;
	int err;

	GETFROMSTACK(ustack, void **, vaddr, 0);
	GETFROMSTACK(ustack, size_t, size, 1);
	GETFROMSTACK(ustack, int, prot, 2);
	GETFROMSTACK(ustack, int, sflags, 3);
	GETFROMSTACK(ustack, int, fildes, 4);
	GETFROMSTACK(ustack, off_t, offs, 5);

	if (sflags < 0) {
		return -EINVAL;
	}
	flags = (unsigned int)sflags;

	size = round_page(size);

	if (vm_mapBelongs(proc, vaddr, sizeof(*vaddr)) < 0) {
		return -EFAULT;
	}

	if ((flags & MAP_ANONYMOUS) != 0U) {
		if ((flags & MAP_PHYSMEM) != 0U) {
			o = VM_OBJ_PHYSMEM;
		}
		else if ((flags & MAP_CONTIGUOUS) != 0U) {
			o = vm_objectContiguous(size);
			if (o == NULL) {
				return -ENOMEM;
			}
		}
		else {
			o = NULL;
		}
	}
	else {
		err = posix_getOid(fildes, &oid);
		if (err < 0) {
			return err;
		}
		err = vm_objectGet(&o, oid);
		if (err < 0) {
			return err;
		}
	}

	flags &= ~(MAP_ANONYMOUS | MAP_CONTIGUOUS | MAP_PHYSMEM);

	/* parasoft-suppress-next-line MISRAC2012-RULE_10_3 "prot is popped from stack -> size of int stays" */
	(*vaddr) = vm_mmap(proc_current()->process->mapp, *vaddr, NULL, size, PROT_USER | (unsigned int)prot, o, (o == NULL) ? -1 : offs, flags);
	(void)vm_objectPut(o);

	if ((*vaddr) == NULL) {
		/* TODO: pass specific errno from vm_mmap */
		return -ENOMEM;
	}

	return EOK;
}


int syscalls_sys_munmap(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	void *vaddr;
	size_t size;
	int err;

	GETFROMSTACK(ustack, void *, vaddr, 0);
	GETFROMSTACK(ustack, size_t, size, 1);

	size = round_page(size);
	err = vm_munmap(proc->mapp, vaddr, size);
	if (err < 0) {
		return err;
	}
	return EOK;
}


int syscalls_sys_mprotect(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	void *vaddr;
	size_t len;
	int prot, err;

	GETFROMSTACK(ustack, void *, vaddr, 0);
	GETFROMSTACK(ustack, size_t, len, 1);
	GETFROMSTACK(ustack, int, prot, 2);

	/* parasoft-suppress-next-line MISRAC2012_RULE_10_3 "prot is popped from stack -> size of int stays" */
	err = vm_mprotect(proc->mapp, vaddr, len, PROT_USER | (unsigned int)prot);
	if (err < 0) {
		return err;
	}
	return EOK;
}


/*
 * Process management
 */


int syscalls_vforksvc(u8 *ustack)
{
	return proc_vfork();
}


int syscalls_sys_fork(u8 *ustack)
{
	return proc_fork();
}


int syscalls_release(u8 *ustack)
{
	return proc_release();
}


int syscalls_sys_spawn(u8 *ustack)
{
	char *path;
	char **argv;
	char **envp;

	/* FIXME pass fields lengths from userspace */

	GETFROMSTACK(ustack, char *, path, 0);
	GETFROMSTACK(ustack, char **, argv, 1);
	GETFROMSTACK(ustack, char **, envp, 2);

	return proc_fileSpawn(path, argv, envp);
}


int syscalls_exec(u8 *ustack)
{
	char *path;
	char **argv;
	char **envp;

	/* FIXME pass fields lengths from userspace */

	GETFROMSTACK(ustack, char *, path, 0);
	GETFROMSTACK(ustack, char **, argv, 1);
	GETFROMSTACK(ustack, char **, envp, 2);

	return proc_execve(path, argv, envp);
}


int syscalls_spawnSyspage(u8 *ustack)
{
	char *imap;
	char *dmap;
	char *name;
	char **argv;

	/* FIXME pass fields lengths from userspace */

	GETFROMSTACK(ustack, char *, imap, 0);
	GETFROMSTACK(ustack, char *, dmap, 1);
	GETFROMSTACK(ustack, char *, name, 2);
	GETFROMSTACK(ustack, char **, argv, 3);

	return proc_syspageSpawnName(imap, dmap, name, argv);
}


int syscalls_sys_exit(u8 *ustack)
{
	int code;

	GETFROMSTACK(ustack, int, code, 0);
	proc_exit(code);
	return EOK;
}


int syscalls_sys_waitpid(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int pid, *status, options;

	GETFROMSTACK(ustack, int, pid, 0);
	GETFROMSTACK(ustack, int *, status, 1);
	GETFROMSTACK(ustack, int, options, 2);

	if ((status != NULL) && (vm_mapBelongs(proc, status, sizeof(*status)) < 0)) {
		return -EFAULT;
	}

	return posix_waitpid(pid, status, (unsigned int)options);
}


int syscalls_threadJoin(u8 *ustack)
{
	int tid;
	time_t timeout;

	GETFROMSTACK(ustack, int, tid, 0);
	GETFROMSTACK(ustack, time_t, timeout, 1);

	return proc_join(tid, timeout);
}


int syscalls_getpid(u8 *ustack)
{
	return process_getPid(proc_current()->process);
}


int syscalls_getppid(u8 *ustack)
{
	return posix_getppid(process_getPid(proc_current()->process));
}


/*
 * Thread management
 */


int syscalls_gettid(u8 *ustack)
{
	return proc_getTid(proc_current());
}


int syscalls_beginthreadex(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	startFn_t start;
	unsigned int priority, stacksz;
	void *stack, *arg;
	int *id;
	int err;

	GETFROMSTACK(ustack, startFn_t, start, 0);
	GETFROMSTACK(ustack, unsigned int, priority, 1);
	GETFROMSTACK(ustack, void *, stack, 2);
	GETFROMSTACK(ustack, unsigned int, stacksz, 3);
	GETFROMSTACK(ustack, void *, arg, 4);
	GETFROMSTACK(ustack, int *, id, 5);

	if ((id != NULL) && (vm_mapBelongs(proc, id, sizeof(*id)) < 0)) {
		return -EFAULT;
	}

	proc_get(proc);

	err = proc_threadCreate(proc, start, id, priority, (size_t)SIZE_KSTACK, stack, stacksz, arg);

	if (err < 0) {
		(void)proc_put(proc);
	}

	return err;
}


__attribute__((noreturn)) void syscalls_endthread(u8 *ustack)
{
	proc_threadEnd();
}


int syscalls_nsleep(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	time_t *sec;
	long int *nsec;
	int clockid;
	int flags;

	GETFROMSTACK(ustack, time_t *, sec, 0);
	GETFROMSTACK(ustack, long int *, nsec, 1);
	GETFROMSTACK(ustack, int, clockid, 2);
	GETFROMSTACK(ustack, int, flags, 3);

	/* Not used right now, future-proofing */
	(void)clockid;

	if (vm_mapBelongs(proc, sec, sizeof(*sec)) < 0) {
		return -EFAULT;
	}

	if (vm_mapBelongs(proc, nsec, sizeof(*nsec)) < 0) {
		return -EFAULT;
	}

	return proc_threadNanoSleep(sec, nsec, (((unsigned int)flags & TIMER_ABSTIME) != 0U) ? 1 : 0);
}


int syscalls_priority(u8 *ustack)
{
	int priority;

	GETFROMSTACK(ustack, int, priority, 0);

	return proc_threadPriority(priority);
}


/*
 * System state info
 */


int syscalls_threadsinfo(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int n, i;
	pid_t ppid;
	threadinfo_t *info;

	GETFROMSTACK(ustack, int, n, 0);
	GETFROMSTACK(ustack, threadinfo_t *, info, 1);

	if (vm_mapBelongs(proc, info, sizeof(*info) * (unsigned int)n) < 0) {
		return -EFAULT;
	}

	n = proc_threadsList(n, info);

	for (i = 0; i < n; ++i) {
		ppid = posix_getppid(info[i].pid);
		if (ppid > 0) {
			info[i].ppid = ppid;
		}
	}

	return n;
}


void syscalls_meminfo(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	meminfo_t *info;

	GETFROMSTACK(ustack, meminfo_t *, info, 0);

	/* TODO: Check subfields too */
	if (vm_mapBelongs(proc, info, sizeof(*info)) >= 0) {
		vm_meminfo(info);
	}
}


int syscalls_syspageprog(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int i;
	size_t sz;
	syspageprog_t *prog;
	const syspage_prog_t *progSys;
	const char *name;

	GETFROMSTACK(ustack, syspageprog_t *, prog, 0);
	GETFROMSTACK(ustack, int, i, 1);

	if ((i >= 0) && (vm_mapBelongs(proc, prog, sizeof(*prog)) < 0)) {
		return -EFAULT;
	}

	sz = syspage_progSize();
	if (i < 0) {
		return (int)sz;
	}

	if (i >= (int)sz) {
		return -EINVAL;
	}

	progSys = syspage_progIdResolve((unsigned int)i);
	if (progSys == NULL) {
		return -EINVAL;
	}

	prog->addr = progSys->start;
	prog->size = progSys->end - progSys->start;

	/* TODO: change syspageprog_t to allocate data for name dynamically */

	name = progSys->argv;
	for (sz = 0U; (name[sz] != '\0') && (name[sz] != ';'); ++sz) {
	}

	sz = min((sizeof(prog->name) - 1U), sz);
	if (*name == 'X') {
		name++;
		sz--;
	}

	hal_memcpy(prog->name, name, sz);
	prog->name[sz] = '\0';

	return EOK;
}


int syscalls_perf_start(u8 *ustack)
{
	unsigned int pid;

	GETFROMSTACK(ustack, unsigned int, pid, 0);

	return perf_start(pid);
}


int syscalls_perf_read(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	void *buffer;
	size_t sz;

	GETFROMSTACK(ustack, void *, buffer, 0);
	GETFROMSTACK(ustack, size_t, sz, 1);

	if (vm_mapBelongs(proc, buffer, sz) < 0) {
		return -EFAULT;
	}

	return perf_read(buffer, sz);
}


int syscalls_perf_finish(u8 *ustack)
{
	return perf_finish();
}

/*
 * Mutexes
 */


int syscalls_phMutexCreate(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	handle_t *h;
	const struct lockAttr *attr;
	int res;

	GETFROMSTACK(ustack, handle_t *, h, 0);
	GETFROMSTACK(ustack, const struct lockAttr *, attr, 1);

	if (vm_mapBelongs(proc, h, sizeof(*h)) < 0) {
		return -EFAULT;
	}

	if (vm_mapBelongs(proc, attr, sizeof(*attr)) < 0) {
		return -EFAULT;
	}

	res = proc_mutexCreate(attr);

	if (res < 0) {
		return res;
	}

	*h = res;
	return EOK;
}


int syscalls_phMutexLock(u8 *ustack)
{
	handle_t h;

	GETFROMSTACK(ustack, handle_t, h, 0);
	return proc_mutexLock(h);
}


int syscalls_mutexTry(u8 *ustack)
{
	handle_t h;

	GETFROMSTACK(ustack, handle_t, h, 0);
	return proc_mutexTry(h);
}


int syscalls_mutexUnlock(u8 *ustack)
{
	handle_t h;

	GETFROMSTACK(ustack, handle_t, h, 0);
	return proc_mutexUnlock(h);
}


/*
 * Conditional variables
 */


int syscalls_phCondCreate(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	const struct condAttr *attr;
	handle_t *h;
	int res;

	GETFROMSTACK(ustack, handle_t *, h, 0);
	GETFROMSTACK(ustack, const struct condAttr *, attr, 1);

	if (vm_mapBelongs(proc, h, sizeof(*h)) < 0) {
		return -EFAULT;
	}

	if (vm_mapBelongs(proc, attr, sizeof(*attr)) < 0) {
		return -EFAULT;
	}

	res = proc_condCreate(attr);
	if (res < 0) {
		return res;
	}

	*h = res;
	return EOK;
}


int syscalls_phCondWait(u8 *ustack)
{
	handle_t h;
	handle_t m;
	time_t timeout;

	GETFROMSTACK(ustack, handle_t, h, 0);
	GETFROMSTACK(ustack, handle_t, m, 1);
	GETFROMSTACK(ustack, time_t, timeout, 2);

	return proc_condWait(h, m, timeout);
}


int syscalls_condSignal(u8 *ustack)
{
	handle_t h;

	GETFROMSTACK(ustack, handle_t, h, 0);
	return proc_condSignal(h);
}


int syscalls_condBroadcast(u8 *ustack)
{
	handle_t h;

	GETFROMSTACK(ustack, handle_t, h, 0);
	return proc_condBroadcast(h);
}


/*
 * Resources
 */


int syscalls_resourceDestroy(u8 *ustack)
{
	handle_t h;

	GETFROMSTACK(ustack, handle_t, h, 0);
	return proc_resourceDestroy(proc_current()->process, h);
}


/*
 * Interrupt management
 */


int syscalls_interrupt(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	unsigned int n;
	userintrFn_t f;
	void *data;
	handle_t cond;
	handle_t *handle;
	int res;

	GETFROMSTACK(ustack, unsigned int, n, 0);
	GETFROMSTACK(ustack, userintrFn_t, f, 1);
	GETFROMSTACK(ustack, void *, data, 2);
	GETFROMSTACK(ustack, handle_t, cond, 3);
	GETFROMSTACK(ustack, handle_t *, handle, 4);

	if ((handle != NULL) && (vm_mapBelongs(proc, handle, sizeof(*handle)) < 0)) {
		return -EFAULT;
	}

	res = userintr_setHandler(n, f, data, cond);
	if (res < 0) {
		return res;
	}

	if (handle != NULL) {
		*handle = res;
	}

	return EOK;
}


/*
 * Message passing
 */


int syscalls_portCreate(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	u32 *port;

	GETFROMSTACK(ustack, u32 *, port, 0);

	if (vm_mapBelongs(proc, port, sizeof(*port)) < 0) {
		return -EFAULT;
	}

	return proc_portCreate(port);
}


void syscalls_portDestroy(u8 *ustack)
{
	u32 port;

	GETFROMSTACK(ustack, u32, port, 0);

	proc_portDestroy(port);
}


int syscalls_portRegister(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	unsigned int port;
	char *name;
	oid_t *oid;

	GETFROMSTACK(ustack, unsigned int, port, 0);
	GETFROMSTACK(ustack, char *, name, 1);
	GETFROMSTACK(ustack, oid_t *, oid, 2);

	/* FIXME: Pass strlen(name) from userspace */

	if (vm_mapBelongs(proc, oid, sizeof(*oid)) < 0) {
		return -EFAULT;
	}

	return proc_portRegister(port, name, oid);
}


int syscalls_msgSend(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	u32 port;
	msg_t *msg;

	GETFROMSTACK(ustack, u32, port, 0);
	GETFROMSTACK(ustack, msg_t *, msg, 1);

	if (vm_mapBelongs(proc, msg, sizeof(*msg)) < 0) {
		return -EFAULT;
	}

	if (msg->i.data != NULL) {
		if (vm_mapBelongs(proc, msg->i.data, msg->i.size) < 0) {
			return -EFAULT;
		}
	}

	if (msg->o.data != NULL) {
		if (vm_mapBelongs(proc, msg->o.data, msg->o.size) < 0) {
			return -EFAULT;
		}
	}

	return proc_send(port, msg);
}


int syscalls_msgRecv(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	u32 port;
	msg_t *msg;
	msg_rid_t *rid;

	GETFROMSTACK(ustack, u32, port, 0);
	GETFROMSTACK(ustack, msg_t *, msg, 1);
	GETFROMSTACK(ustack, msg_rid_t *, rid, 2);

	if (vm_mapBelongs(proc, msg, sizeof(*msg)) < 0) {
		return -EFAULT;
	}

	if (vm_mapBelongs(proc, rid, sizeof(*rid)) < 0) {
		return -EFAULT;
	}

	return proc_recv(port, msg, rid);
}


int syscalls_msgRespond(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	u32 port;
	msg_t *msg;
	msg_rid_t rid;

	GETFROMSTACK(ustack, u32, port, 0);
	GETFROMSTACK(ustack, msg_t *, msg, 1);
	GETFROMSTACK(ustack, msg_rid_t, rid, 2);

	if (vm_mapBelongs(proc, msg, sizeof(*msg)) < 0) {
		return -EFAULT;
	}

#ifndef NOMMU /* o.data has client memory pointer on NOMMU */
	if (msg->o.data != NULL) {
		if (vm_mapBelongs(proc, msg->o.data, msg->o.size) < 0) {
			return -EFAULT;
		}
	}
#endif

	return proc_respond(port, msg, rid);
}


int syscalls_lookup(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	char *name;
	oid_t *file, *dev;

	GETFROMSTACK(ustack, char *, name, 0);
	GETFROMSTACK(ustack, oid_t *, file, 1);
	GETFROMSTACK(ustack, oid_t *, dev, 2);

	/* FIXME: Pass strlen(name) from userspace */

	if ((file != NULL) && (vm_mapBelongs(proc, file, sizeof(*file)) < 0)) {
		return -EFAULT;
	}

	if ((dev != NULL) && (vm_mapBelongs(proc, dev, sizeof(*dev)) < 0)) {
		return -EFAULT;
	}

	return proc_portLookup(name, file, dev);
}


/*
 * Time management
 */


int syscalls_gettime(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	time_t *praw, *poffs;

	GETFROMSTACK(ustack, time_t *, praw, 0);
	GETFROMSTACK(ustack, time_t *, poffs, 1);

	if ((praw != NULL) && (vm_mapBelongs(proc, praw, sizeof(*praw)) < 0)) {
		return -EFAULT;
	}

	if ((poffs != NULL) && (vm_mapBelongs(proc, poffs, sizeof(*poffs)) < 0)) {
		return -EFAULT;
	}

	proc_gettime(praw, poffs);

	return EOK;
}


int syscalls_settime(u8 *ustack)
{
	time_t offs;

	GETFROMSTACK(ustack, time_t, offs, 0);

	return proc_settime(offs);
}


/*
 * Power management
 */


void syscalls_keepidle(u8 *ustack)
{
	int t;

	GETFROMSTACK(ustack, int, t, 0);
	hal_cpuSetDevBusy(t);
}


/*
 * Memory map dump
 */


void syscalls_mmdump(char *ustack)
{
	vm_mapDump(NULL);
}


/*
 * Platform specific call
 */


int syscalls_platformctl(u8 *ustack)
{
	/* FIXME: Allow access to sizeof(platformctl_t) to allow checks */
	void *ptr;
	GETFROMSTACK(ustack, void *, ptr, 0);
	return hal_platformctl(ptr);
}


/*
 * Watchdog
 */


void syscalls_wdgreload(u8 *ustack)
{
	hal_wdgReload();
}


addr_t syscalls_va2pa(u8 *ustack)
{
	void *va;

	GETFROMSTACK(ustack, void *, va, 0);

	return (pmap_resolve(proc_current()->process->pmapp, (void *)((ptr_t)va & ~0xfffU)) & ~0xfffU) + ((ptr_t)va & 0xfffU);
}


int syscalls_signalHandle(u8 *ustack)
{
	sighandlerFn_t handler;
	unsigned int mask, mmask;
	thread_t *thread;

	GETFROMSTACK(ustack, sighandlerFn_t, handler, 0);
	GETFROMSTACK(ustack, unsigned int, mask, 1);
	GETFROMSTACK(ustack, unsigned int, mmask, 2);

	thread = proc_current();
	thread->process->sigmask = (mask & mmask) | (thread->process->sigmask & ~mmask);
	thread->process->sighandler = handler;

	return EOK;
}


int syscalls_signalPost(u8 *ustack)
{
	int pid, tid, signal, err;
	process_t *proc;
	thread_t *t = NULL;

	GETFROMSTACK(ustack, int, pid, 0);
	GETFROMSTACK(ustack, int, tid, 1);
	GETFROMSTACK(ustack, int, signal, 2);

	proc = proc_find(pid);
	if (proc == NULL) {
		return -EINVAL;
	}

	if (tid >= 0) {
		t = threads_findThread(tid);
		if (t == NULL) {
			(void)proc_put(proc);
			return -EINVAL;
		}
	}

	if ((t != NULL) && (t->process != proc)) {
		(void)proc_put(proc);
		threads_put(t);
		return -EINVAL;
	}

	err = threads_sigpost(proc, t, signal);

	(void)proc_put(proc);
	if (t != NULL) {
		threads_put(t);
	}

	return err;
}


unsigned int syscalls_signalMask(u8 *ustack)
{
	unsigned int mask, mmask, old;
	thread_t *t;

	GETFROMSTACK(ustack, unsigned int, mask, 0);
	GETFROMSTACK(ustack, unsigned int, mmask, 1);

	t = proc_current();

	old = t->sigmask;
	t->sigmask = (mask & mmask) | (t->sigmask & ~mmask);

	return old;
}


int syscalls_signalSuspend(u8 *ustack)
{
	unsigned int mask;
	GETFROMSTACK(ustack, unsigned int, mask, 0);

	return threads_sigsuspend(mask);
}


void syscalls_sigreturn(u8 *ustack)
{
	thread_t *t = proc_current();
	cpu_context_t *ctx;
	unsigned int oldmask;

	GETFROMSTACK(ustack, unsigned int, oldmask, 0);
	GETFROMSTACK(ustack, cpu_context_t *, ctx, 1);

	hal_cpuDisableInterrupts();
	hal_cpuSigreturn(t->kstack + t->kstacksz, ustack, &ctx);

	t->sigmask = oldmask;

	/* TODO: check if return address belongs to user mapped memory */
	if (hal_cpuSupervisorMode(ctx) != 0) {
		proc_kill(t->process);
	}

	proc_longjmp(ctx);

	__builtin_unreachable();
}

/* POSIX compatibility syscalls */


int syscalls_sys_open(u8 *ustack)
{
	const char *filename;
	int oflag;

	/* FIXME: pass strlen(filename) from userspace */

	GETFROMSTACK(ustack, const char *, filename, 0);
	GETFROMSTACK(ustack, int, oflag, 1);

	return posix_open(filename, oflag, ustack);
}


int syscalls_sys_close(u8 *ustack)
{
	int fildes;

	GETFROMSTACK(ustack, int, fildes, 0);

	return posix_close(fildes);
}


ssize_t syscalls_sys_read(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fildes;
	void *buf;
	size_t nbyte;
	off_t offset;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, void *, buf, 1);
	GETFROMSTACK(ustack, size_t, nbyte, 2);
	GETFROMSTACK(ustack, off_t, offset, 3);

	if ((buf == NULL) && (nbyte != 0U)) {
		return -EFAULT;
	}

	if ((buf != NULL) && (nbyte != 0U) && (vm_mapBelongs(proc, buf, nbyte) < 0)) {
		return -EFAULT;
	}

	return posix_read(fildes, buf, nbyte, offset);
}


ssize_t syscalls_sys_write(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fildes;
	void *buf;
	size_t nbyte;
	off_t offset;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, void *, buf, 1);
	GETFROMSTACK(ustack, size_t, nbyte, 2);
	GETFROMSTACK(ustack, off_t, offset, 3);

	if ((buf == NULL) && (nbyte != 0U)) {
		return -EFAULT;
	}

	if ((buf != NULL) && (nbyte != 0U) && (vm_mapBelongs(proc, buf, nbyte) < 0)) {
		return -EFAULT;
	}

	return posix_write(fildes, buf, nbyte, offset);
}


int syscalls_sys_dup(u8 *ustack)
{
	int fildes;

	GETFROMSTACK(ustack, int, fildes, 0);

	return posix_dup(fildes);
}


int syscalls_sys_dup2(u8 *ustack)
{
	int fildes;
	int fildes2;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, int, fildes2, 1);

	return posix_dup2(fildes, fildes2);
}


int syscalls_sys_link(u8 *ustack)
{
	const char *path1;
	const char *path2;

	/* FIXME pass strlen(path1) and strlen(path2) from userspace */

	GETFROMSTACK(ustack, const char *, path1, 0);
	GETFROMSTACK(ustack, const char *, path2, 1);

	return posix_link(path1, path2);
}


int syscalls_sys_unlink(u8 *ustack)
{
	const char *pathname;

	/* FIXME: pass strlen(pathname) from userspace */

	GETFROMSTACK(ustack, const char *, pathname, 0);

	return posix_unlink(pathname);
}


int syscalls_sys_lseek(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fildes;
	off_t *offset;
	int whence;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, off_t *, offset, 1);
	GETFROMSTACK(ustack, int, whence, 2);

	if (vm_mapBelongs(proc, offset, sizeof(*offset)) < 0) {
		return -EFAULT;
	}

	return posix_lseek(fildes, offset, whence);
}


int syscalls_sys_ftruncate(u8 *ustack)
{
	int fildes;
	off_t length;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, off_t, length, 1);

	return posix_ftruncate(fildes, length);
}


int syscalls_sys_fcntl(u8 *ustack)
{
	int fd;
	unsigned int cmd;

	GETFROMSTACK(ustack, int, fd, 0);
	GETFROMSTACK(ustack, unsigned int, cmd, 1);

	return posix_fcntl(fd, cmd, ustack);
}


int syscalls_sys_pipe(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int *fildes;

	GETFROMSTACK(ustack, int *, fildes, 0);

	if (vm_mapBelongs(proc, fildes, sizeof(*fildes) * 2U) < 0) {
		return -EFAULT;
	}

	return posix_pipe(fildes);
}


int syscalls_sys_mkfifo(u8 *ustack)
{
	const char *path;
	mode_t mode;

	/* FIXME: pass strlen(path) from userspace */

	GETFROMSTACK(ustack, const char *, path, 0);
	GETFROMSTACK(ustack, mode_t, mode, 1);

	return posix_mkfifo(path, mode);
}


int syscalls_sys_fstat(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fd;
	struct stat *buf;

	GETFROMSTACK(ustack, int, fd, 0);
	GETFROMSTACK(ustack, struct stat *, buf, 1);

	if (vm_mapBelongs(proc, buf, sizeof(*buf)) < 0) {
		return -EFAULT;
	}

	return posix_fstat(fd, buf);
}


int syscalls_sys_statvfs(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fd;
	const char *path;
	struct statvfs *buf;

	GETFROMSTACK(ustack, const char *, path, 0);
	GETFROMSTACK(ustack, int, fd, 1);
	GETFROMSTACK(ustack, struct statvfs *, buf, 2);

	if (vm_mapBelongs(proc, buf, sizeof(*buf)) < 0) {
		return -EFAULT;
	}

	return posix_statvfs(path, fd, buf);
}


int syscalls_sys_fsync(u8 *ustack)
{
	int fd;

	GETFROMSTACK(ustack, int, fd, 0);

	return posix_fsync(fd);
}


int syscalls_sys_chmod(u8 *ustack)
{
	const char *path;
	mode_t mode;

	/* FIXME: pass strlen(path) from userspace */

	GETFROMSTACK(ustack, const char *, path, 0);
	GETFROMSTACK(ustack, mode_t, mode, 1);

	return posix_chmod(path, mode);
}


int syscalls_sys_accept(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t *, address_len, 2);

	if (address != NULL) {
		if (vm_mapBelongs(proc, address_len, sizeof(*address_len)) < 0) {
			return -EFAULT;
		}

		if (vm_mapBelongs(proc, address, *address_len) < 0) {
			return -EFAULT;
		}
	}

	return posix_accept(socket, address, address_len);
}


int syscalls_sys_accept4(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;
	int flags;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t *, address_len, 2);
	GETFROMSTACK(ustack, int, flags, 3);

	if (address != NULL) {
		if (vm_mapBelongs(proc, address_len, sizeof(*address_len)) < 0) {
			return -EFAULT;
		}

		if (vm_mapBelongs(proc, address, *address_len) < 0) {
			return -EFAULT;
		}
	}

	return posix_accept4(socket, address, address_len, flags);
}


int syscalls_sys_bind(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	const struct sockaddr *address;
	socklen_t address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, const struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t, address_len, 2);

	if (vm_mapBelongs(proc, address, address_len) < 0) {
		return -EFAULT;
	}

	return posix_bind(socket, address, address_len);
}


int syscalls_sys_connect(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	const struct sockaddr *address;
	socklen_t address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, const struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t, address_len, 2);

	if (vm_mapBelongs(proc, address, address_len) < 0) {
		return -EFAULT;
	}

	return posix_connect(socket, address, address_len);
}


int syscalls_sys_gethostname(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	char *name;
	size_t namelen;

	GETFROMSTACK(ustack, char *, name, 0);
	GETFROMSTACK(ustack, size_t, namelen, 1);

	if (vm_mapBelongs(proc, name, namelen) < 0) {
		return -EFAULT;
	}

	return posix_gethostname(name, namelen);
}


int syscalls_sys_getpeername(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t *, address_len, 2);

	if (vm_mapBelongs(proc, address_len, sizeof(*address_len)) < 0) {
		return -EFAULT;
	}

	if (vm_mapBelongs(proc, address, *address_len) < 0) {
		return -EFAULT;
	}

	return posix_getpeername(socket, address, address_len);
}


int syscalls_sys_getsockname(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t *, address_len, 2);

	if (vm_mapBelongs(proc, address_len, sizeof(*address_len)) < 0) {
		return -EFAULT;
	}

	if (vm_mapBelongs(proc, address, *address_len) < 0) {
		return -EFAULT;
	}

	return posix_getsockname(socket, address, address_len);
}


int syscalls_sys_getsockopt(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	int level;
	int optname;
	void *optval;
	socklen_t *optlen;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, int, level, 1);
	GETFROMSTACK(ustack, int, optname, 2);
	GETFROMSTACK(ustack, void *, optval, 3);
	GETFROMSTACK(ustack, socklen_t *, optlen, 4);

	if (optval != NULL) {
		if (vm_mapBelongs(proc, optlen, sizeof(*optlen)) < 0) {
			return -EFAULT;
		}

		if (vm_mapBelongs(proc, optval, *optlen) < 0) {
			return -EFAULT;
		}
	}

	return posix_getsockopt(socket, level, optname, optval, optlen);
}


int syscalls_sys_listen(u8 *ustack)
{
	int socket;
	int backlog;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, int, backlog, 1);

	return posix_listen(socket, backlog);
}


ssize_t syscalls_sys_recvfrom(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	void *message;
	size_t length;
	int flags;
	struct sockaddr *src_addr;
	socklen_t *src_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, void *, message, 1);
	GETFROMSTACK(ustack, size_t, length, 2);
	GETFROMSTACK(ustack, int, flags, 3);
	GETFROMSTACK(ustack, struct sockaddr *, src_addr, 4);
	GETFROMSTACK(ustack, socklen_t *, src_len, 5);

	if (vm_mapBelongs(proc, message, length) < 0) {
		return -EFAULT;
	}

	if (src_addr != NULL) {
		if (vm_mapBelongs(proc, src_len, sizeof(*src_len)) < 0) {
			return -EFAULT;
		}

		if (vm_mapBelongs(proc, src_addr, *src_len) < 0) {
			return -EFAULT;
		}
	}

	return posix_recvfrom(socket, message, length, flags, src_addr, src_len);
}


ssize_t syscalls_sys_sendto(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	const void *message;
	size_t length;
	int flags;
	const struct sockaddr *dest_addr;
	socklen_t dest_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, const void *, message, 1);
	GETFROMSTACK(ustack, size_t, length, 2);
	GETFROMSTACK(ustack, int, flags, 3);
	GETFROMSTACK(ustack, const struct sockaddr *, dest_addr, 4);
	GETFROMSTACK(ustack, socklen_t, dest_len, 5);

	if (vm_mapBelongs(proc, message, length) < 0) {
		return -EFAULT;
	}

	if ((dest_addr != NULL) && (vm_mapBelongs(proc, dest_addr, dest_len) < 0)) {
		return -EFAULT;
	}

	return posix_sendto(socket, message, length, flags, dest_addr, dest_len);
}


ssize_t syscalls_sys_recvmsg(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	struct msghdr *msg;
	int flags;
	size_t i;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct msghdr *, msg, 1);
	GETFROMSTACK(ustack, int, flags, 2);

	if (vm_mapBelongs(proc, msg, sizeof(*msg)) < 0) {
		return -EFAULT;
	}

	if ((msg->msg_iovlen != 0) && (vm_mapBelongs(proc, msg->msg_iov, sizeof(*msg->msg_iov) * (size_t)msg->msg_iovlen) < 0)) {
		return -EFAULT;
	}

	for (i = 0; i < (size_t)msg->msg_iovlen; ++i) {
		if ((msg->msg_iov[i].iov_base != NULL) && (vm_mapBelongs(proc, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len) < 0)) {
			return -EFAULT;
		}
	}

	if ((msg->msg_control != NULL) && (vm_mapBelongs(proc, msg->msg_control, msg->msg_controllen) < 0)) {
		return -EFAULT;
	}

	if ((msg->msg_name != NULL) && (vm_mapBelongs(proc, msg->msg_name, msg->msg_namelen) < 0)) {
		return -EFAULT;
	}

	return posix_recvmsg(socket, msg, flags);
}


ssize_t syscalls_sys_sendmsg(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	const struct msghdr *msg;
	int flags;
	size_t i;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, const struct msghdr *, msg, 1);
	GETFROMSTACK(ustack, int, flags, 2);

	if (vm_mapBelongs(proc, msg, sizeof(*msg)) < 0) {
		return -EFAULT;
	}

	if ((msg->msg_iovlen != 0) && (vm_mapBelongs(proc, msg->msg_iov, sizeof(*msg->msg_iov) * (size_t)msg->msg_iovlen) < 0)) {
		return -EFAULT;
	}

	for (i = 0; i < (size_t)msg->msg_iovlen; ++i) {
		if ((msg->msg_iov[i].iov_base != NULL) && (vm_mapBelongs(proc, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len) < 0)) {
			return -EFAULT;
		}
	}

	if ((msg->msg_control != NULL) && (vm_mapBelongs(proc, msg->msg_control, msg->msg_controllen) < 0)) {
		return -EFAULT;
	}

	if ((msg->msg_name != NULL) && (vm_mapBelongs(proc, msg->msg_name, msg->msg_namelen) < 0)) {
		return -EFAULT;
	}

	return posix_sendmsg(socket, msg, flags);
}


int syscalls_sys_socket(u8 *ustack)
{
	int domain;
	int type;
	int protocol;

	GETFROMSTACK(ustack, int, domain, 0);
	GETFROMSTACK(ustack, int, type, 1);
	GETFROMSTACK(ustack, int, protocol, 2);

	return posix_socket(domain, type, protocol);
}


int syscalls_sys_socketpair(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int domain;
	int type;
	int protocol;
	int *sv;

	GETFROMSTACK(ustack, int, domain, 0);
	GETFROMSTACK(ustack, int, type, 1);
	GETFROMSTACK(ustack, int, protocol, 2);
	GETFROMSTACK(ustack, int *, sv, 3);

	if (vm_mapBelongs(proc, sv, sizeof(*sv) * 2U) < 0) {
		return -EFAULT;
	}

	return posix_socketpair(domain, type, protocol, sv);
}


int syscalls_sys_shutdown(u8 *ustack)
{
	int socket;
	int how;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, int, how, 1);

	return posix_shutdown(socket, how);
}


int syscalls_sys_sethostname(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	const char *name;
	size_t namelen;

	GETFROMSTACK(ustack, const char *, name, 0);
	GETFROMSTACK(ustack, size_t, namelen, 1);

	if (vm_mapBelongs(proc, name, namelen) < 0) {
		return -EFAULT;
	}

	return posix_sethostname(name, namelen);
}


int syscalls_sys_setsockopt(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	int level;
	int optname;
	const void *optval;
	socklen_t optlen;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, int, level, 1);
	GETFROMSTACK(ustack, int, optname, 2);
	GETFROMSTACK(ustack, const void *, optval, 3);
	GETFROMSTACK(ustack, socklen_t, optlen, 4);

	if ((optval != NULL) && (optlen != 0U) && (vm_mapBelongs(proc, optval, optlen) < 0)) {
		return -EFAULT;
	}

	return posix_setsockopt(socket, level, optname, optval, optlen);
}


int syscalls_sys_ioctl(u8 *ustack)
{
	int fildes;
	unsigned long request;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, unsigned long, request, 1);

	return posix_ioctl(fildes, request, ustack);
}


int syscalls_sys_poll(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	struct pollfd *fds;
	nfds_t nfds;
	int timeout_ms;

	GETFROMSTACK(ustack, struct pollfd *, fds, 0);
	GETFROMSTACK(ustack, nfds_t, nfds, 1);
	GETFROMSTACK(ustack, int, timeout_ms, 2);

	if (vm_mapBelongs(proc, fds, sizeof(*fds) * nfds) < 0) {
		return -EFAULT;
	}

	return posix_poll(fds, nfds, timeout_ms);
}


int syscalls_sys_futimens(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fildes;
	const struct timespec *times;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, const struct timespec *, times, 1);

	if ((times != NULL) && (vm_mapBelongs(proc, times, sizeof(*times)) < 0)) {
		return -EFAULT;
	}

	return posix_futimens(fildes, times);
}


int syscalls_sys_tkill(u8 *ustack)
{
	pid_t pid;
	int tid;
	int sig;

	GETFROMSTACK(ustack, pid_t, pid, 0);
	GETFROMSTACK(ustack, int, tid, 1);
	GETFROMSTACK(ustack, int, sig, 2);

	return posix_tkill(pid, tid, sig);
}


int syscalls_sys_setpgid(u8 *ustack)
{
	pid_t pid, pgid;

	GETFROMSTACK(ustack, pid_t, pid, 0);
	GETFROMSTACK(ustack, pid_t, pgid, 1);

	return posix_setpgid(pid, pgid);
}


pid_t syscalls_sys_getpgid(u8 *ustack)
{
	pid_t pid;

	GETFROMSTACK(ustack, pid_t, pid, 0);

	return posix_getpgid(pid);
}


int syscalls_sys_setpgrp(u8 *ustack)
{
	return posix_setpgid(0, 0);
}


pid_t syscalls_sys_getpgrp(u8 *ustack)
{
	return posix_getpgid(0);
}


pid_t syscalls_sys_setsid(u8 *ustack)
{
	return posix_setsid();
}


void syscalls_sbi_putchar(u8 *ustack)
{
#ifdef __TARGET_RISCV64
	char c;
	GETFROMSTACK(ustack, char, c, 0);
	(void)hal_sbiPutchar(c);
#endif
}


int syscalls_sbi_getchar(u8 *ustack)
{
#ifdef __TARGET_RISCV64
	return (int)hal_sbiGetchar();
#else
	return -1;
#endif
}


int syscalls_sys_uname(u8 *ustack)
{
	struct utsname *name;
	GETFROMSTACK(ustack, struct utsname *, name, 0);

	if (vm_mapBelongs(proc_current()->process, name, sizeof(*name)) < 0) {
		return -EFAULT;
	}

	return posix_uname(name);
}


/*
 * Empty syscall
 */


int syscalls_notimplemented(void)
{
	return -ENOTTY;
}


/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "Syscalls are in different types" */
const void *const syscalls[] = { SYSCALLS(SYSCALLS_NAME) };


void *syscalls_dispatch(int n, u8 *ustack, cpu_context_t *ctx)
{
	void *retval;

	if (n >= (int)(sizeof(syscalls) / sizeof(syscalls[0]))) {
		return (void *)-EINVAL;
	}

	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 MISRAC2012-RULE_11_8 "Related to previous suppression" */
	retval = ((void *(*)(u8 *arg))syscalls[n])(ustack);

	if (proc_current()->exit != 0U) {
		proc_threadEnd();
	}

	threads_setupUserReturn(retval, ctx);

	return retval;
}


void _syscalls_init(void)
{
	lib_printf("syscalls: Initializing syscall table [%d]\n", sizeof(syscalls) / sizeof(syscalls[0]));
}

/* parasoft-end-suppress MISRAC2012-RULE_8_4 */
