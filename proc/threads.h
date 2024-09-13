/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Thread manager
 *
 * Copyright 2012-2013, 2017 Phoenix Systems
 * Copyright 2001, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Jacek Popko
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_THREADS_H_
#define _PROC_THREADS_H_

#include "hal/hal.h"
#include "lib/lib.h"
#include "include/sysinfo.h"
#include "process.h"
#include "lock.h"

#define MAX_TID        MAX_ID
#define THREAD_END     1
#define THREAD_END_NOW 2

/* Parent thread states */
enum { PREFORK = 0, FORKING = 1, FORKED };

/* Child thread states */
enum { OWNSTACK = 0, PARENTSTACK };

enum { READY = 0, SLEEP, GHOST };


typedef struct _thread_t {
	struct _thread_t *next;
	struct _thread_t *prev;
	struct _lock_t *locks;

	rbnode_t sleeplinkage;
	idnode_t idlinkage;

	struct _process_t *process;
	struct _thread_t *procnext;
	struct _thread_t *procprev;

	int refs;
	struct _thread_t *blocking;

	struct _thread_t **wait;
	volatile time_t wakeup;

	unsigned priorityBase : 4;
	unsigned priority : 4;
	unsigned state : 2;
	unsigned exit : 2;
	unsigned interruptible : 1;

	unsigned sigmask;
	unsigned sigpend;

	time_t stick;
	time_t utick;

	void *kstack;
	size_t kstacksz;
	char *ustack;

	ptr_t tlsPtr;

	/* for vfork/exec */
	void *parentkstack, *execkstack;
	void *execdata;

	time_t readyTime;
	time_t maxWait;

	time_t startTime;
	time_t cpuTime;
	time_t lastTime;

	cpu_context_t *context;
	cpu_context_t *longjmpctx;
} thread_t;


static inline int proc_getTid(thread_t *t)
{
	return t->idlinkage.id;
}


extern int perf_start(unsigned pid);


extern int perf_read(void *buffer, size_t bufsz);


extern int perf_finish(void);


extern void perf_fork(process_t *p);


extern void perf_kill(process_t *p);


extern void perf_exec(process_t *p, char *path);


extern thread_t *proc_current(void);


extern void threads_canaryInit(thread_t *t, void *ustack);


extern int proc_threadCreate(process_t *process, void (*start)(void *), int *id, unsigned int priority, size_t kstacksz, void *stack, size_t stacksz, void *arg, void *tls);


extern int proc_threadPriority(int priority);


extern void proc_threadProtect(void);


extern void proc_threadUnprotect(void);


extern void proc_threadEnd(void);


extern int proc_threadJoin(unsigned int id);


extern void proc_threadDestroy(thread_t *t);


extern void proc_threadsDestroy(thread_t **threads);


extern int proc_waitpid(int pid, int *stat, int options);


extern int proc_join(int tid, time_t timeout);


extern void proc_changeMap(process_t *proc, vm_map_t *map, vm_map_t *imap, pmap_t *pmap);


extern int proc_threadsList(int n, threadinfo_t *info);


extern void proc_zombie(process_t *proc);


extern int proc_threadSleep(time_t us);


extern int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp);


extern int proc_threadWaitInterruptible(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp);


extern int proc_threadWakeup(thread_t **queue);


extern void proc_threadWakeupYield(thread_t **queue);


extern int proc_threadBroadcast(thread_t **queue);


extern void proc_threadBroadcastYield(thread_t **queue);


extern void proc_tlsSetPtr(void *tlsPtr);


extern void proc_tlsSetReg(void *tlsReg);


extern int threads_getCpuTime(thread_t *t);


extern thread_t *threads_findThread(int tid);


extern void threads_put(thread_t *thread);


extern time_t proc_uptime(void);


extern void proc_gettime(time_t *raw, time_t *offs);


extern int proc_settime(time_t offs);


extern time_t proc_nextWakeup(void);


extern void proc_longjmp(cpu_context_t *ctx);


extern void proc_threadsDump(unsigned int priority);


extern int _threads_init(vm_map_t *kmap, vm_object_t *kernel);


extern int threads_sigpost(process_t *process, thread_t *thread, int sig);


extern void threads_setupUserReturn(void *retval);


#endif
