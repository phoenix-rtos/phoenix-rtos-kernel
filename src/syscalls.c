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

#include HAL
#include "../include/errno.h"
#include "../include/sysinfo.h"
#include "../include/mman.h"
#include "../include/syscalls.h"
#include "lib/lib.h"
#include "proc/proc.h"
#include "vm/object.h"

#define SYSCALLS_NAME(name) syscalls_##name,

/*
 * Kernel
 */


void syscalls_debug(void *ustack)
{
	char *s;

	GETFROMSTACK(ustack, char *, s, 0);
	hal_consolePrint(ATTR_USER, s);
}


/*
 * Memory management
 */


void *syscalls_mmap(void *ustack)
{
	void *vaddr;
	size_t size;
	int prot, flags;
	oid_t *oid;
	offs_t offs;
	vm_object_t *o;

	GETFROMSTACK(ustack, void *, vaddr, 0);
	GETFROMSTACK(ustack, size_t, size, 1);
	GETFROMSTACK(ustack, int, prot, 2);
	GETFROMSTACK(ustack, int, flags, 3);
	GETFROMSTACK(ustack, oid_t *, oid, 4);
	GETFROMSTACK(ustack, offs_t, offs, 5);

	if (oid == (void *)-1)
		o = (void *)-1;
	else if (oid == NULL)
		o = NULL;
	else if (vm_objectGet(&o, *oid) != EOK)
		return NULL;

	vaddr = vm_mmap(proc_current()->process->mapp, vaddr, NULL, size, PROT_USER | prot, o, (o == NULL) ? -1 : offs, flags);
	vm_objectPut(o);

	/* vm_mapDump(proc_current()->process->mapp); */
	if (vaddr == NULL)
		return (void *)-1;

	return vaddr;
}


void syscalls_munmap(void *ustack)
{
	void *vaddr;
	size_t size;

	GETFROMSTACK(ustack, void *, vaddr, 0);
	GETFROMSTACK(ustack, size_t, size, 1);

	vm_munmap(proc_current()->process->mapp, vaddr, size);
}


/*
 * Process management
 */


int syscalls_vforksvc(void *ustack)
{
	return proc_vfork();
}


int syscalls_fork(void *ustack)
{
	return posix_fork();
}


int syscalls_exec(void *ustack)
{
	char *path;
	char **argv;
	char **envp;

	GETFROMSTACK(ustack, char *, path, 0);
	GETFROMSTACK(ustack, char **, argv, 1);
	GETFROMSTACK(ustack, char **, envp, 2);

	return proc_execve(NULL, path, argv, envp);
}


int syscalls_exit(void *ustack)
{
	int code;

	GETFROMSTACK(ustack, int, code, 0);
	proc_exit(code);
	return EOK;
}


int syscalls_waitpid(void *ustack)
{
	int pid, *stat, options;

	GETFROMSTACK(ustack, int, pid, 0);
	GETFROMSTACK(ustack, int *, stat, 1);
	GETFROMSTACK(ustack, int, options, 2);

	return proc_waitpid(pid, stat, options);
}


int syscalls_getpid(void *ustack)
{
	return proc_current()->process->id;
}


int syscalls_getppid(void *ustack)
{
	if (proc_current()->process->parent == NULL)
		return -EINVAL;
	return proc_current()->process->parent->id;
}


/*
 * Thread management
 */


int syscalls_gettid(void *ustack)
{
	return (int)proc_current()->id;
}


int syscalls_beginthreadex(void *ustack)
{
	void (*start)(void *);
	unsigned int priority, stacksz;
	void *stack, *arg;
	unsigned int *id;

	GETFROMSTACK(ustack, void *, start, 0);
	GETFROMSTACK(ustack, unsigned int, priority, 1);
	GETFROMSTACK(ustack, void *, stack, 2);
	GETFROMSTACK(ustack, unsigned int, stacksz, 3);
	GETFROMSTACK(ustack, void *, arg, 4);
	GETFROMSTACK(ustack, unsigned int *, id, 5);

	return proc_threadCreate(proc_current()->process, start, id, priority, SIZE_KSTACK, stack, stacksz, arg);
}


int syscalls_endthread(void *ustack)
{
	proc_threadDestroy();
	return EOK;
}


int syscalls_usleep(void *ustack)
{
	unsigned int us;

	GETFROMSTACK(ustack, unsigned int, us, 0);
	return proc_threadSleep(us);
}


int syscalls_priority(void *ustack)
{
	int priority;
	thread_t *thread;

	GETFROMSTACK(ustack, int, priority, 0);

	thread = proc_current();

	if (priority == -1)
		return thread->priority;
	else if (priority >= 0 && priority <= 7)
		return thread->priority = priority;

	return -EINVAL;
}


/*
 * System state info
 */


int syscalls_threadsinfo(void *ustack)
{
	int n;
	threadinfo_t *info;

	GETFROMSTACK(ustack, int, n, 0);
	GETFROMSTACK(ustack, threadinfo_t *, info, 1);

	return proc_threadsList(n, info);
}


void syscalls_meminfo(void *ustack)
{
	meminfo_t *info;

	GETFROMSTACK(ustack, meminfo_t *, info, 0);

	vm_meminfo(info);
}


int syscalls_syspageprog(void *ustack)
{
#ifndef NOMMU
	int i;
	syspageprog_t *prog;

	GETFROMSTACK(ustack, syspageprog_t *, prog, 0);
	GETFROMSTACK(ustack, int, i, 1);

	if (i < 0)
		return syspage->progssz;

	if (i >= syspage->progssz)
		return -EINVAL;

	prog->addr = syspage->progs[i].start;
	prog->size = syspage->progs[i].end - syspage->progs[i].start;
	hal_memcpy(prog->name, syspage->progs[i].cmdline, sizeof(syspage->progs[i].cmdline));

	return EOK;
#else
	return -EINVAL;
#endif
}


/*
 * Mutexes
 */


int syscalls_mutexCreate(void *ustack)
{
	unsigned int *h;

	GETFROMSTACK(ustack, unsigned int *, h, 0);
	return proc_mutexCreate(h);
}


int syscalls_mutexLock(void *ustack)
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

	GETFROMSTACK(ustack, unsigned int *, h, 0);
	return proc_condCreate(h);
}


int syscalls_condWait(void *ustack)
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
	process_t *proc;

	proc = proc_current()->process;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_condSignal(proc, h);
}


/*
 * Resources
 */


int syscalls_resourceDestroy(void *ustack)
{
	unsigned int h;

	GETFROMSTACK(ustack, unsigned int, h, 0);
	return proc_resourceFree(h);
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

	GETFROMSTACK(ustack, unsigned int, n, 0);
	GETFROMSTACK(ustack, void *, f, 1);
	GETFROMSTACK(ustack, void *, data, 2);
	GETFROMSTACK(ustack, unsigned int, cond, 3);
	GETFROMSTACK(ustack, unsigned int *, handle, 4);

	return userintr_setHandler(n, f, data, cond, handle);
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
	unsigned int *rid;

	GETFROMSTACK(ustack, u32, port, 0);
	GETFROMSTACK(ustack, msg_t *, msg, 1);
	GETFROMSTACK(ustack, unsigned int *, rid, 2);

	return proc_recv(port, msg, rid);
}


int syscalls_msgRespond(void *ustack)
{
	u32 port;
	msg_t *msg;
	unsigned int rid;

	GETFROMSTACK(ustack, u32, port, 0);
	GETFROMSTACK(ustack, msg_t *, msg, 1);
	GETFROMSTACK(ustack, unsigned int, rid, 2);

	return proc_respond(port, msg, rid);
}


int syscalls_lookup(void *ustack)
{
	char *name;
	oid_t *oid;

	GETFROMSTACK(ustack, char *, name, 0);
	GETFROMSTACK(ustack, oid_t *, oid, 1);

	return proc_portLookup(name, oid);
}


/*
 * Time management
 */


int syscalls_gettime(void *ustack)
{
	time_t *praw, *poffs, raw, offs;

	GETFROMSTACK(ustack, time_t *, praw, 0);
	GETFROMSTACK(ustack, time_t *, poffs, 1);

	proc_gettime(&raw, &offs);

	if (praw != NULL)
		(*praw) = raw;

	if (poffs != NULL)
		(*poffs) = offs;

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
#ifdef CPU_STM32
	int t;

	GETFROMSTACK(ustack, int, t, 0);
	hal_cpuSetDevBusy(t);
#endif
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

	return (pmap_resolve(&proc_current()->process->mapp->pmap, (void *)((unsigned long)va & ~0xfff)) & ~0xfff) + ((unsigned long)va & 0xfff);
}


void syscalls_signalHandle(void *ustack)
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
}


int syscalls_signalPost(void *ustack)
{
	int pid, signal, err;
	process_t *proc;

	GETFROMSTACK(ustack, int, pid, 0);
	GETFROMSTACK(ustack, int, signal, 1);

	if ((proc = proc_find(pid)) == NULL)
		return -EINVAL;

	err = proc_sigpost(proc, NULL, signal);
	hal_cpuReschedule(NULL);
	return err;
}


void syscalls_signalReturn(void *ustack)
{
	int signal;
	GETFROMSTACK(ustack, int, signal, 0);
	proc_sigreturn(signal);
}


void syscalls_signalMask(void *ustack)
{
	unsigned mask, mmask;
	thread_t *t;

	GETFROMSTACK(ustack, unsigned, mask, 0);
	GETFROMSTACK(ustack, unsigned, mmask, 1);

	t = proc_current();
	t->sigmask = (mask & mmask) | (t->sigmask & ~mmask);
}


/* POSIX compatibility syscalls */

int syscalls_open_absolute(char *ustack)
{
	const char *filename;
	int oflag;

	GETFROMSTACK(ustack, const char *, filename, 0);
	GETFROMSTACK(ustack, int, oflag, 1);

	return posix_open(filename, oflag, ustack);
}


int syscalls_close(char *ustack)
{
	int fildes;

	GETFROMSTACK(ustack, int, fildes, 0);

	return posix_close(fildes);
}


int syscalls_read(char *ustack)
{
	int fildes;
	void *buf;
	size_t nbyte;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, void *, buf, 1);
	GETFROMSTACK(ustack, size_t, nbyte, 2);

	return posix_read(fildes, buf, nbyte);
}


int syscalls_write(char *ustack)
{
	int fildes;
	void *buf;
	size_t nbyte;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, void *, buf, 1);
	GETFROMSTACK(ustack, size_t, nbyte, 2);

	return posix_write(fildes, buf, nbyte);
}


int syscalls_dup(char *ustack)
{
	int fildes;

	GETFROMSTACK(ustack, int, fildes, 0);

	return posix_dup(fildes);
}


int syscalls_dup2(char *ustack)
{
	int fildes;
	int fildes2;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, int, fildes2, 1);

	return posix_dup2(fildes, fildes2);
}


int syscalls_link_absolute(char *ustack)
{
	const char *path1;
	const char *path2;

	GETFROMSTACK(ustack, const char *, path1, 0);
	GETFROMSTACK(ustack, const char *, path2, 1);

	return posix_link(path1, path2);
}


int syscalls_unlink_absolute(char *ustack)
{
	const char *pathname;

	GETFROMSTACK(ustack, const char *, pathname, 0);

	return posix_unlink(pathname);
}


off_t syscalls_lseek(char *ustack)
{
	int fildes;
	off_t offset;
	int whence;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, off_t, offset, 1);
	GETFROMSTACK(ustack, int, whence, 2);

	return posix_lseek(fildes, offset, whence);
}


int syscalls_ftruncate(char *ustack)
{
	int fildes;
	off_t length;

	GETFROMSTACK(ustack, int, fildes, 0);
	GETFROMSTACK(ustack, off_t, length, 1);

	return posix_ftruncate(fildes, length);
}


int syscalls_fcntl(char *ustack)
{
	unsigned int fd;
	unsigned int cmd;

	GETFROMSTACK(ustack, unsigned int, fd, 0);
	GETFROMSTACK(ustack, unsigned int, cmd, 1);

	return posix_fcntl(fd, cmd, ustack);
}


int syscalls_pipe(char *ustack)
{
	int *fildes;

	GETFROMSTACK(ustack, int *, fildes, 0);

	return posix_pipe(fildes);
}


int syscalls_mkfifo_absolute(char *ustack)
{
	const char *path;
	mode_t mode;

	GETFROMSTACK(ustack, const char *, path, 0);
	GETFROMSTACK(ustack, mode_t, mode, 1);

	return posix_mkfifo(path, mode);
}


/*
 * Empty syscall
 */


int syscalls_notimplemented(void)
{
	return -ENOTTY;
}


const void * const syscalls[] = { SYSCALLS(SYSCALLS_NAME) };


void *syscalls_dispatch(int n, char *ustack)
{
	void *retval;

	if (n > sizeof(syscalls) / sizeof(syscalls[0]))
		return (void *)-EINVAL;

	proc_threadProtect();
	retval = ((void *(*)(char *))syscalls[n])(ustack);
	proc_threadUnprotect();

	return retval;
}


void _syscalls_init(void)
{
	lib_printf("syscalls: Initializing syscall table [%d]\n", sizeof(syscalls) / sizeof(syscalls[0]));
}
