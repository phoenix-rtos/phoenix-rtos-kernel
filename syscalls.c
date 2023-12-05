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

#include "hal/hal.h"
#include "include/errno.h"
#include "include/sysinfo.h"
#include "include/mman.h"
#include "include/syscalls.h"
#include "include/posix.h"
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


void syscalls_debug(void *ustack)
{
	const char *s;

	GETFROMSTACK(ustack, const char *, s, 0);
	hal_consolePrint(ATTR_USER, s);
}


/*
 * Memory management
 */


void *syscalls_mmap(void *ustack)
{
	void *vaddr;
	size_t size;
	int prot, flags, fildes;
	off_t offs;
	vm_object_t *o;
	oid_t oid;

	GETFROMSTACK(ustack, void *, vaddr, 0);
	GETFROMSTACK(ustack, size_t, size, 1);
	GETFROMSTACK(ustack, int, prot, 2);
	GETFROMSTACK(ustack, int, flags, 3);
	GETFROMSTACK(ustack, int, fildes, 4);
	GETFROMSTACK(ustack, off_t, offs, 5);

	if ((flags & MAP_ANONYMOUS) != 0) {
		if ((flags & MAP_PHYSMEM) != 0) {
			o = VM_OBJ_PHYSMEM;
		}
		else if ((flags & MAP_CONTIGUOUS) != 0) {
			o = vm_objectContiguous(size);
			if (o == NULL) {
				return MAP_FAILED;
			}
		}
		else {
			o = NULL;
		}
	}
	else {
		if (posix_getOid(fildes, &oid) != EOK) {
			return MAP_FAILED;
		}
		if (vm_objectGet(&o, oid) != EOK) {
			return MAP_FAILED;
		}
	}

	vaddr = vm_mmap(proc_current()->process->mapp, vaddr, NULL, size, PROT_USER | prot, o, (o == NULL) ? -1 : offs, flags);
	vm_objectPut(o);

	if (vaddr == NULL) {
		return MAP_FAILED;
	}

	return vaddr;
}


int syscalls_munmap(void *ustack)
{
	void *vaddr;
	size_t size;
	int err;

	GETFROMSTACK(ustack, void *, vaddr, 0);
	GETFROMSTACK(ustack, size_t, size, 1);

	err = vm_munmap(proc_current()->process->mapp, vaddr, size);
	if (err < 0) {
		return err;
	}
	return EOK;
}


int syscalls_mprotect(void *ustack)
{
	void *vaddr;
	size_t len;
	int prot, err;

	GETFROMSTACK(ustack, void *, vaddr, 0);
	GETFROMSTACK(ustack, size_t, len, 1);
	GETFROMSTACK(ustack, int, prot, 2);

	err = vm_mprotect(proc_current()->process->mapp, vaddr, len, PROT_USER | prot);
	if (err < 0) {
		return err;
	}
	return EOK;
}


/*
 * Process management
 */


int syscalls_vforksvc(void *ustack)
{
	return proc_vfork();
}


int syscalls_sys_fork(void *ustack)
{
	return proc_fork();
}


int syscalls_release(void *ustack)
{
	return proc_release();
}


int syscalls_sys_spawn(void *ustack)
{
	char *path;
	char **argv;
	char **envp;

	GETFROMSTACK(ustack, char *, path, 0);
	GETFROMSTACK(ustack, char **, argv, 1);
	GETFROMSTACK(ustack, char **, envp, 2);

	return proc_fileSpawn(path, argv, envp);
}


int syscalls_exec(void *ustack)
{
	char *path;
	char **argv;
	char **envp;

	GETFROMSTACK(ustack, char *, path, 0);
	GETFROMSTACK(ustack, char **, argv, 1);
	GETFROMSTACK(ustack, char **, envp, 2);

	return proc_execve(path, argv, envp);
}


int syscalls_spawnSyspage(void *ustack)
{
	char *imap;
	char *dmap;
	char *name;
	char **argv;

	GETFROMSTACK(ustack, char *, imap, 0);
	GETFROMSTACK(ustack, char *, dmap, 1);
	GETFROMSTACK(ustack, char *, name, 2);
	GETFROMSTACK(ustack, char **, argv, 3);

	return proc_syspageSpawnName(imap, dmap, name, argv);
}


int syscalls_sys_exit(void *ustack)
{
	int code;

	GETFROMSTACK(ustack, int, code, 0);
	proc_exit(code);
	return EOK;
}


int syscalls_sys_waitpid(void *ustack)
{
	int pid, *stat, options;

	GETFROMSTACK(ustack, int, pid, 0);
	GETFROMSTACK(ustack, int *, stat, 1);
	GETFROMSTACK(ustack, int, options, 2);

	return posix_waitpid(pid, stat, options);
}


int syscalls_threadJoin(void *ustack)
{
	int tid;
	time_t timeout;

	GETFROMSTACK(ustack, int, tid, 0);
	GETFROMSTACK(ustack, time_t, timeout, 1);

	return proc_join(tid, timeout);
}


int syscalls_getpid(void *ustack)
{
	return process_getPid(proc_current()->process);
}


int syscalls_getppid(void *ustack)
{
	return posix_getppid(process_getPid(proc_current()->process));
}


/*
 * Thread management
 */


int syscalls_gettid(void *ustack)
{
	return proc_getTid(proc_current());
}


int syscalls_beginthreadex(void *ustack)
{
	void (*start)(void *);
	unsigned int priority, stacksz;
	void *stack, *arg;
	unsigned int *id;
	process_t *p;
	int err;

	GETFROMSTACK(ustack, void *, start, 0);
	GETFROMSTACK(ustack, unsigned int, priority, 1);
	GETFROMSTACK(ustack, void *, stack, 2);
	GETFROMSTACK(ustack, unsigned int, stacksz, 3);
	GETFROMSTACK(ustack, void *, arg, 4);
	GETFROMSTACK(ustack, unsigned int *, id, 5);

	p = proc_current()->process;
	if (p != NULL) {
		proc_get(p);
	}

	err = proc_threadCreate(p, start, id, priority, SIZE_KSTACK, stack, stacksz, arg);

	if ((p != NULL) && (err < 0)) {
		proc_put(p);
	}

	return err;
}


int syscalls_endthread(void *ustack)
{
	proc_threadEnd();
	return EOK;
}


int syscalls_nsleep(void *ustack)
{
	time_t *sec;
	long int *nsec;
	time_t start, us, stop, diff;
	int ret;

	GETFROMSTACK(ustack, time_t *, sec, 0);
	GETFROMSTACK(ustack, long int *, nsec, 1);

	/* TODO - user pointer check */
	if ((sec == NULL) || (nsec == NULL)) {
		return -EINVAL;
	}

	/* FIXME - time_t should be signed and we should check for *sec < 0 */
	if (((*nsec) < 0) || ((*nsec) >= (1000 * 1000 * 1000))) {
		return -EINVAL;
	}

	proc_gettime(&start, NULL);

	us = ((*sec) * 1000 * 1000) + (((*nsec) + 999) / 1000);

	ret = proc_threadSleep(us);

	*sec = 0;
	*nsec = 0;

	if (ret == -EINTR) {
		proc_gettime(&stop, NULL);
		diff = stop - start;
		if (diff < us) {
			*sec = diff / (1000 * 1000);
			*nsec = (diff % (1000 * 1000)) * 1000;
		}
	}

	return ret;
}


int syscalls_priority(void *ustack)
{
	int priority;

	GETFROMSTACK(ustack, int, priority, 0);

	return proc_threadPriority(priority);
}


/*
 * System state info
 */


int syscalls_threadsinfo(void *ustack)
{
	int n, i;
	pid_t ppid;
	threadinfo_t *info;

	GETFROMSTACK(ustack, int, n, 0);
	GETFROMSTACK(ustack, threadinfo_t *, info, 1);

	n = proc_threadsList(n, info);

	for (i = 0; i < n; ++i) {
		ppid = posix_getppid(info[i].pid);
		if (ppid > 0) {
			info[i].ppid = ppid;
		}
	}

	return n;
}


void syscalls_meminfo(void *ustack)
{
	meminfo_t *info;

	GETFROMSTACK(ustack, meminfo_t *, info, 0);

	vm_meminfo(info);
}


int syscalls_syspageprog(void *ustack)
{
	int i;
	size_t sz;
	syspageprog_t *prog;
	const syspage_prog_t *progSys;
	const char *name;

	GETFROMSTACK(ustack, syspageprog_t *, prog, 0);
	GETFROMSTACK(ustack, int, i, 1);

	sz = syspage_progSize();
	if (i < 0) {
		return sz;
	}

	if (i >= sz) {
		return -EINVAL;
	}

	progSys = syspage_progIdResolve(i);
	if (progSys == NULL) {
		return -EINVAL;
	}

	prog->addr = progSys->start;
	prog->size = progSys->end - progSys->start;

	/* TODO: change syspageprog_t to allocate data for name dynamically */

	name = progSys->argv;
	for (sz = 0; (name[sz] != '\0') && (name[sz] != ';'); ++sz) {
	}

	sz = min(sizeof(prog->name) - 1, sz);
	if (*name == 'X') {
		name++;
		sz--;
	}

	hal_memcpy(prog->name, name, sz);
	prog->name[sz] = '\0';

	return EOK;
}


int syscalls_perf_start(void *ustack)
{
	unsigned pid;

	GETFROMSTACK(ustack, unsigned, pid, 0);

	return perf_start(pid);
}


int syscalls_perf_read(void *ustack)
{
	void *buffer;
	size_t sz;

	GETFROMSTACK(ustack, void *, buffer, 0);
	GETFROMSTACK(ustack, size_t, sz, 1);

	return perf_read(buffer, sz);
}


int syscalls_perf_finish(void *ustack)
{
	return perf_finish();
}

/*
 * Mutexes
 */


int syscalls_mutexCreate(void *ustack)
{
	unsigned int *h;
	int res;

	GETFROMSTACK(ustack, unsigned int *, h, 0);

	if ((res = proc_mutexCreate()) < 0)
		return res;

	*h = res;
	return EOK;
}


int syscalls_phMutexLock(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_mutexLock(h);
}


int syscalls_mutexTry(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_mutexTry(h);
}


int syscalls_mutexUnlock(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_mutexUnlock(h);
}


/*
 * Conditional variables
 */


int syscalls_condCreate(void *ustack)
{
	unsigned int *h;
	int res;

	GETFROMSTACK(ustack, unsigned int *, h, 0);

	if ((res = proc_condCreate()) < 0)
		return res;

	*h = res;
	return EOK;
}


int syscalls_phCondWait(void *ustack)
{
	unsigned int h;
	unsigned int m;
	time_t timeout;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	GETFROMSTACK(ustack, unsigned int, m, 1);
	GETFROMSTACK(ustack, time_t, timeout, 2);

	return proc_condWait(h, m, timeout);
}


int syscalls_condSignal(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_condSignal(h);
}


int syscalls_condBroadcast(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_condBroadcast(h);
}


/*
 * Resources
 */


int syscalls_resourceDestroy(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_resourceDestroy(proc_current()->process, h);
}


/*
 * Interrupt management
 */


int syscalls_interrupt(void *ustack)
{
	unsigned int n;
	void *f;
	void *data;
	unsigned int cond;
	unsigned int *handle;
	int res;

	GETFROMSTACK(ustack, unsigned int, n, 0);
	GETFROMSTACK(ustack, void *, f, 1);
	GETFROMSTACK(ustack, void *, data, 2);
	GETFROMSTACK(ustack, unsigned int, cond, 3);
	GETFROMSTACK(ustack, unsigned int *, handle, 4);

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


int syscalls_portCreate(void *ustack)
{
	u32 *port;

	GETFROMSTACK(ustack, u32 *, port, 0);

	return proc_portCreate(port);
}


void syscalls_portDestroy(void *ustack)
{
	u32 port;

	GETFROMSTACK(ustack, u32, port, 0);

	proc_portDestroy(port);
}


u32 syscalls_portRegister(void *ustack)
{
	unsigned int port;
	char *name;
	oid_t *oid;

	GETFROMSTACK(ustack, unsigned int, port, 0);
	GETFROMSTACK(ustack, char *, name, 1);
	GETFROMSTACK(ustack, oid_t *, oid, 2);

	return proc_portRegister(port, name, oid);
}


int syscalls_msgSend(void *ustack)
{
	u32 port;
	msg_t *msg;

	GETFROMSTACK(ustack, u32, port, 0);
	GETFROMSTACK(ustack, msg_t *, msg, 1);

	return proc_send(port, msg);
}


int syscalls_msgRecv(void *ustack)
{
	u32 port;
	msg_t *msg;
	msg_rid_t *rid;

	GETFROMSTACK(ustack, u32, port, 0);
	GETFROMSTACK(ustack, msg_t *, msg, 1);
	GETFROMSTACK(ustack, msg_rid_t *, rid, 2);

	return proc_recv(port, msg, rid);
}


int syscalls_msgRespond(void *ustack)
{
	u32 port;
	msg_t *msg;
	msg_rid_t rid;

	GETFROMSTACK(ustack, u32, port, 0);
	GETFROMSTACK(ustack, msg_t *, msg, 1);
	GETFROMSTACK(ustack, msg_rid_t, rid, 2);

	return proc_respond(port, msg, rid);
}


int syscalls_lookup(void *ustack)
{
	char *name;
	oid_t *file, *dev;

	GETFROMSTACK(ustack, char *, name, 0);
	GETFROMSTACK(ustack, oid_t *, file, 1);
	GETFROMSTACK(ustack, oid_t *, dev, 2);

	return proc_portLookup(name, file, dev);
}


/*
 * Time management
 */


int syscalls_gettime(void *ustack)
{
	time_t *praw, *poffs;

	GETFROMSTACK(ustack, time_t *, praw, 0);
	GETFROMSTACK(ustack, time_t *, poffs, 1);

	proc_gettime(praw, poffs);

	return EOK;
}


int syscalls_settime(void *ustack)
{
	time_t offs;

	GETFROMSTACK(ustack, time_t, offs, 0);

	return proc_settime(offs);
}


/*
 * Power management
 */


void syscalls_keepidle(void *ustack)
{
	int t;

	GETFROMSTACK(ustack, int, t, 0);
	hal_cpuSetDevBusy(t);
}


/*
 * Memory map dump
 */


void syscalls_mmdump(void *ustack)
{
	vm_mapDump(NULL);
}


/*
 * Platform specific call
 */


int syscalls_platformctl(void *ustack)
{
	void *ptr;
	GETFROMSTACK(ustack, void *, ptr, 0);
	return hal_platformctl(ptr);
}


/*
 * Watchdog
 */


void syscalls_wdgreload(void *ustack)
{
	hal_wdgReload();
}


/*
 * File operations
 */


int syscalls_fileAdd(void *ustack)
{
	unsigned int *h;
	oid_t *oid;
	unsigned int mode;

	GETFROMSTACK(ustack, unsigned int *, h, 0);
	GETFROMSTACK(ustack, oid_t *, oid, 1);
	GETFROMSTACK(ustack, unsigned int, mode, 2);

	return proc_fileAdd(h, oid, mode);
}


int syscalls_fileSet(void *ustack)
{
	unsigned int h;
	char flags;
	oid_t *oid;
	offs_t offs;
	unsigned mode;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	GETFROMSTACK(ustack, char, flags, 1);
	GETFROMSTACK(ustack, oid_t *, oid, 2);
	GETFROMSTACK(ustack, offs_t, offs, 3);
	GETFROMSTACK(ustack, unsigned, mode, 4);

	return proc_fileSet(h, flags, oid, offs, mode);
}


int syscalls_fileGet(void *ustack)
{
	unsigned int h;
	int flags;
	oid_t *oid;
	offs_t *offs;
	unsigned *mode;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	GETFROMSTACK(ustack, int, flags, 1);
	GETFROMSTACK(ustack, oid_t *, oid, 2);
	GETFROMSTACK(ustack, offs_t *, offs, 3);
	GETFROMSTACK(ustack, unsigned *, mode, 4);

	return proc_fileGet(h, flags, oid, offs, mode);
}


int syscalls_fileRemove(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);

	return proc_fileRemove(h);
}


addr_t syscalls_va2pa(void *ustack)
{
	void *va;

	GETFROMSTACK(ustack, void *, va, 0);

	return (pmap_resolve(proc_current()->process->pmapp, (void *)((ptr_t)va & ~0xfff)) & ~0xfff) + ((ptr_t)va & 0xfff);
}


int syscalls_signalHandle(void *ustack)
{
	void *handler;
	unsigned mask, mmask;
	thread_t *thread;

	GETFROMSTACK(ustack, void *, handler, 0);
	GETFROMSTACK(ustack, unsigned, mask, 1);
	GETFROMSTACK(ustack, unsigned, mmask, 2);

	thread = proc_current();
	thread->process->sigmask = (mask & mmask) | (thread->process->sigmask & ~mmask);
	thread->process->sighandler = handler;

	return EOK;
}


int syscalls_signalPost(void *ustack)
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
			proc_put(proc);
			return -EINVAL;
		}
	}

	if ((t != NULL) && (t->process != proc)) {
		proc_put(proc);
		threads_put(t);
		return -EINVAL;
	}

	err = threads_sigpost(proc, t, signal);

	proc_put(proc);
	if (t != NULL) {
		threads_put(t);
	}

	return err;
}


unsigned int syscalls_signalMask(void *ustack)
{
	unsigned mask, mmask, old;
	thread_t *t;

	GETFROMSTACK(ustack, unsigned, mask, 0);
	GETFROMSTACK(ustack, unsigned, mmask, 1);

	t = proc_current();

	old = t->sigmask;
	t->sigmask = (mask & mmask) | (t->sigmask & ~mmask);

	return old;
}


int syscalls_signalSuspend(void *ustack)
{
	unsigned int mask, old;
	int ret;
	thread_t *t;

	GETFROMSTACK(ustack, unsigned, mask, 0);

	t = proc_current();

	old = t->sigmask;
	t->sigmask = mask;

	do {
		ret = proc_threadSleep(1ULL << 52);
	} while (ret != -EINTR);

	t->sigmask = old;

	return ret;
}


void syscalls_sigreturn(void *ustack)
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

	hal_longjmp(ctx);

	/* Not reached */
}

/* POSIX compatibility syscalls */


int syscalls_sys_open(char *ustack)
{
	const char *filename;
	int oflag;

	GETFROMSTACK(ustack, const char *, filename, 0);
	GETFROMSTACK(ustack, int, oflag, 1);

	return posix_open(filename, oflag, ustack);
}


int syscalls_sys_close(char *ustack)
{
	int fildes;

	GETFROMSTACK(ustack, int, fildes, 0);

	return posix_close(fildes);
}


ssize_t syscalls_sys_read(char *ustack)
{
	int fildes;
	void *buf;
	size_t nbyte;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, void *, buf, 1);
	GETFROMSTACK(ustack, size_t, nbyte, 2);

	return posix_read(fildes, buf, nbyte);
}


ssize_t syscalls_sys_write(char *ustack)
{
	int fildes;
	void *buf;
	size_t nbyte;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, void *, buf, 1);
	GETFROMSTACK(ustack, size_t, nbyte, 2);

	return posix_write(fildes, buf, nbyte);
}


int syscalls_sys_dup(char *ustack)
{
	int fildes;

	GETFROMSTACK(ustack, int, fildes, 0);

	return posix_dup(fildes);
}


int syscalls_sys_dup2(char *ustack)
{
	int fildes;
	int fildes2;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, int, fildes2, 1);

	return posix_dup2(fildes, fildes2);
}


int syscalls_sys_link(char *ustack)
{
	const char *path1;
	const char *path2;

	GETFROMSTACK(ustack, const char *, path1, 0);
	GETFROMSTACK(ustack, const char *, path2, 1);

	return posix_link(path1, path2);
}


int syscalls_sys_unlink(char *ustack)
{
	const char *pathname;

	GETFROMSTACK(ustack, const char *, pathname, 0);

	return posix_unlink(pathname);
}


int syscalls_sys_lseek(char *ustack)
{
	int fildes;
	off_t *offset;
	int whence;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, off_t *, offset, 1);
	GETFROMSTACK(ustack, int, whence, 2);

	/* TODO verify pointer */
	if (offset == NULL) {
		return -EINVAL;
	}

	return posix_lseek(fildes, offset, whence);
}


int syscalls_sys_ftruncate(char *ustack)
{
	int fildes;
	off_t length;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, off_t, length, 1);

	return posix_ftruncate(fildes, length);
}


int syscalls_sys_fcntl(char *ustack)
{
	unsigned int fd;
	unsigned int cmd;

	GETFROMSTACK(ustack, unsigned int, fd, 0);
	GETFROMSTACK(ustack, unsigned int, cmd, 1);

	return posix_fcntl(fd, cmd, ustack);
}


int syscalls_sys_pipe(char *ustack)
{
	int *fildes;

	GETFROMSTACK(ustack, int *, fildes, 0);

	return posix_pipe(fildes);
}


int syscalls_sys_mkfifo(char *ustack)
{
	const char *path;
	mode_t mode;

	GETFROMSTACK(ustack, const char *, path, 0);
	GETFROMSTACK(ustack, mode_t, mode, 1);

	return posix_mkfifo(path, mode);
}


int syscalls_sys_fstat(char *ustack)
{
	int fd;
	struct stat *buf;

	GETFROMSTACK(ustack, int, fd, 0);
	GETFROMSTACK(ustack, struct stat *, buf, 1);

	return posix_fstat(fd, buf);
}


int syscalls_sys_fsync(char *ustack)
{
	int fd;

	GETFROMSTACK(ustack, int, fd, 0);

	return posix_fsync(fd);
}


int syscalls_sys_chmod(char *ustack)
{
	const char *path;
	mode_t mode;

	GETFROMSTACK(ustack, const char *, path, 0);
	GETFROMSTACK(ustack, mode_t, mode, 1);

	return posix_chmod(path, mode);
}


int syscalls_sys_accept(char *ustack)
{
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t *, address_len, 2);

	return posix_accept(socket, address, address_len);
}


int syscalls_sys_accept4(char *ustack)
{
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;
	int flags;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t *, address_len, 2);
	GETFROMSTACK(ustack, int, flags, 3);

	return posix_accept4(socket, address, address_len, flags);
}


int syscalls_sys_bind(char *ustack)
{
	int socket;
	const struct sockaddr *address;
	socklen_t address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, const struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t, address_len, 2);

	return posix_bind(socket, address, address_len);
}


int syscalls_sys_connect(char *ustack)
{
	int socket;
	const struct sockaddr *address;
	socklen_t address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, const struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t, address_len, 2);

	return posix_connect(socket, address, address_len);
}


int syscalls_sys_gethostname(char *ustack)
{
	char *name;
	size_t namelen;

	GETFROMSTACK(ustack, char *, name, 0);
	GETFROMSTACK(ustack, size_t, namelen, 1);

	return posix_gethostname(name, namelen);
}


int syscalls_sys_getpeername(char *ustack)
{
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t *, address_len, 2);

	return posix_getpeername(socket, address, address_len);
}


int syscalls_sys_getsockname(char *ustack)
{
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct sockaddr *, address, 1);
	GETFROMSTACK(ustack, socklen_t *, address_len, 2);

	return posix_getsockname(socket, address, address_len);
}


int syscalls_sys_getsockopt(char *ustack)
{
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

	return posix_getsockopt(socket, level, optname, optval, optlen);
}


int syscalls_sys_listen(char *ustack)
{
	int socket;
	int backlog;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, int, backlog, 1);

	return posix_listen(socket, backlog);
}


ssize_t syscalls_sys_recvfrom(char *ustack)
{
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

	return posix_recvfrom(socket, message, length, flags, src_addr, src_len);
}


ssize_t syscalls_sys_sendto(char *ustack)
{
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

	return posix_sendto(socket, message, length, flags, dest_addr, dest_len);
}


ssize_t syscalls_sys_recvmsg(char *ustack)
{
	int socket;
	struct msghdr *msg;
	int flags;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, struct msghdr *, msg, 1);
	GETFROMSTACK(ustack, int, flags, 2);

	return posix_recvmsg(socket, msg, flags);
}


ssize_t syscalls_sys_sendmsg(char *ustack)
{
	int socket;
	const struct msghdr *msg;
	int flags;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, const struct msghdr *, msg, 1);
	GETFROMSTACK(ustack, int, flags, 2);

	return posix_sendmsg(socket, msg, flags);
}


int syscalls_sys_socket(char *ustack)
{
	int domain;
	int type;
	int protocol;

	GETFROMSTACK(ustack, int, domain, 0);
	GETFROMSTACK(ustack, int, type, 1);
	GETFROMSTACK(ustack, int, protocol, 2);

	return posix_socket(domain, type, protocol);
}


int syscalls_sys_socketpair(char *ustack)
{
	int domain;
	int type;
	int protocol;
	int *sv;

	GETFROMSTACK(ustack, int, domain, 0);
	GETFROMSTACK(ustack, int, type, 1);
	GETFROMSTACK(ustack, int, protocol, 2);
	GETFROMSTACK(ustack, int *, sv, 3);

	return posix_socketpair(domain, type, protocol, sv);
}


int syscalls_sys_shutdown(char *ustack)
{
	int socket;
	int how;

	GETFROMSTACK(ustack, int, socket, 0);
	GETFROMSTACK(ustack, int, how, 1);

	return posix_shutdown(socket, how);
}


int syscalls_sys_sethostname(char *ustack)
{
	const char *name;
	size_t namelen;

	GETFROMSTACK(ustack, const char *, name, 0);
	GETFROMSTACK(ustack, size_t, namelen, 1);

	return posix_sethostname(name, namelen);
}


int syscalls_sys_setsockopt(char *ustack)
{
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

	return posix_setsockopt(socket, level, optname, optval, optlen);
}


int syscalls_sys_ioctl(char *ustack)
{
	int fildes;
	unsigned long request;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, unsigned long, request, 1);

	return posix_ioctl(fildes, request, ustack);
}


int syscalls_sys_poll(char *ustack)
{
	struct pollfd *fds;
	nfds_t nfds;
	int timeout_ms;

	GETFROMSTACK(ustack, struct pollfd *, fds, 0);
	GETFROMSTACK(ustack, nfds_t, nfds, 1);
	GETFROMSTACK(ustack, int, timeout_ms, 2);

	return posix_poll(fds, nfds, timeout_ms);
}


int syscalls_sys_futimens(char *ustack)
{
	int fildes;
	const struct timespec *times;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, const struct timespec *, times, 1);

	return posix_futimens(fildes, times);
}


int syscalls_sys_tkill(char *ustack)
{
	pid_t pid;
	int tid;
	int sig;

	GETFROMSTACK(ustack, pid_t, pid, 0);
	GETFROMSTACK(ustack, int, tid, 1);
	GETFROMSTACK(ustack, int, sig, 2);

	return posix_tkill(pid, tid, sig);
}


int syscalls_sys_setpgid(char *ustack)
{
	pid_t pid, pgid;

	GETFROMSTACK(ustack, pid_t, pid, 0);
	GETFROMSTACK(ustack, pid_t, pgid, 1);

	return posix_setpgid(pid, pgid);
}


pid_t syscalls_sys_getpgid(char *ustack)
{
	pid_t pid;

	GETFROMSTACK(ustack, pid_t, pid, 0);

	return posix_getpgid(pid);
}


int syscalls_sys_setpgrp(char *ustack)
{
	return posix_setpgid(0, 0);
}


pid_t syscalls_sys_getpgrp(char *ustack)
{
	return posix_getpgid(0);
}


pid_t syscalls_sys_setsid(char *ustack)
{
	return posix_setsid();
}


void syscalls_sbi_putchar(char *ustack)
{
#ifdef __TARGET_RISCV64
	char c;
	GETFROMSTACK(ustack, char, c, 0);
	sbi_ecall(SBI_PUTCHAR, 0, c, 0, 0, 0, 0, 0);
#endif
}


int syscalls_sbi_getchar(char *ustack)
{
#ifdef __TARGET_RISCV64
	sbiret_t ret;
	ret = sbi_ecall(SBI_GETCHAR, 0, 0, 0, 0, 0, 0, 0);
	return (int)ret.error;
#else
	return -1;
#endif
}


/*
 * Empty syscall
 */


int syscalls_notimplemented(void)
{
	return -ENOTTY;
}


const void *const syscalls[] = { SYSCALLS(SYSCALLS_NAME) };
const char *const syscall_strings[] = { SYSCALLS(SYSCALLS_STRING) };


void *syscalls_dispatch(int n, char *ustack)
{
	void *retval;

	if (n >= (sizeof(syscalls) / sizeof(syscalls[0]))) {
		return (void *)-EINVAL;
	}

	retval = ((void *(*)(char *))syscalls[n])(ustack);

	if (proc_current()->exit != 0) {
		proc_threadEnd();
	}

	threads_setupUserReturn(retval);

	return retval;
}


void _syscalls_init(void)
{
	lib_printf("syscalls: Initializing syscall table [%d]\n", sizeof(syscalls) / sizeof(syscalls[0]));
}
