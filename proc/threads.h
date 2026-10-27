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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_PROC_THREADS_H_
#define _PH_PROC_THREADS_H_

#include "hal/hal.h"
#include "include/msg.h"
#include "lib/lib.h"
#include "include/sysinfo.h"
#include "include/sched.h"
#include "process.h"
#include "lock.h"
#include "sc.h"

#ifdef DEBUG_THREADS
#define LIB_ASSERT_THREADS(condition, fmt, ...) LIB_ASSERT_ALWAYS((condition), (fmt), ##__VA_ARGS__)
#else
#define LIB_ASSERT_THREADS(condition, fmt, ...)
#endif

#define NPRIOS   16U
#define MAX_PRIO (NPRIOS - 1U) /* Maximum priority value, of the lowest criticality (prio=0 is of the HIGHEST) */

#define MAX_TID        MAX_ID
#define THREAD_END     1U
#define THREAD_END_NOW 2U

/* Thread states */
#define READY            0U
#define SLEEP            1U
#define GHOST            2U
#define BLOCKED_ON_RECV  3U
#define BLOCKED_ON_SEND  4U
#define BLOCKED_ON_REPLY 5U

#define WAIT_NOTHING 0U
#define WAIT_LOCK    1U /* a PROTO_INHERIT lock_t, we wait for its owner */
#define WAIT_RECV    2U /* thread_t, a busy port receiver */
#define WAIT_CALL    3U /* thread_t, the IPC callee serving our message */
#define WAIT_REAP    4U /* thread_t, a ghost waits for the thread that will destroy it */


typedef struct _xferBuf_t {
	/* TODO: do this as a private impl state */
#ifndef NOMMU
	void *win;

	size_t boffs;
	page_t *bp;

	size_t eoffs;
	page_t *ep;
#else
	void *orig;
#endif

	void *w;
	size_t size;
	vm_map_t *map;
} xferBuf_t;


#define IPC_IN_FROM_RECV    (1 << 0)
#define IPC_OUT_FROM_RECV   (1 << 1)
#define IPC_IN_DATA_MAPPED  (1 << 2)
#define IPC_OUT_DATA_MAPPED (1 << 3)

typedef struct _thread_t {
	struct _lock_t *locks;

	rbnode_t sleeplinkage;
	idnode_t idlinkage;

	struct _process_t *process;
	struct _thread_t *procnext;
	struct _thread_t *procprev;

	/* TODO lots of pointers... maybe it'd possible to optimize this? */
	struct _thread_t *qnext;
	struct _thread_t *qprev;

	struct _thread_t *gnext; /* threads_common.ghosts linkage */
	struct _thread_t *gprev;

	int refs;

	struct _thread_t **wait;
	time_t wakeup;

	sched_context_t *scOwn;     /* thread's base SC - never donated away */
	sched_context_t *scActive;  /* SC currently being consumed (used by scheduler) */
	sched_context_t *scDonated; /* SCs donated to the thread */

	void *blockedOn;           /* blockedKind declares type */
	sched_context_t *scPushed; /* SC this thread donated through _sc_push(), if any */

	/* busy receivers may be boosted by clients waiting for ready receiver in _portWait() */
	struct _thread_t *boosters;
	struct _thread_t *boostNext;
	struct _thread_t *boostPrev;

	struct _port_t *waitPort; /* port whose queue this thread waits on, if any */
	struct _port_t *busyPort; /* port whose message this thread serves, if any */
	struct _thread_t *bnext;  /* busyPort->busy linkage */
	struct _thread_t *bprev;
	struct _thread_t *granted; /* receiver this sender was given, if any */

	unsigned int prioceilingLocks : 8;
	unsigned int priorityBase : 4;
	unsigned int priority : 4;
	unsigned int state : 4;
	unsigned int blockedKind : 3;
	unsigned int interruptible : 1;
	unsigned int exit : 2;
	unsigned int ipcInterrupted : 1;
	unsigned int onReady : 1; /* 1 iff linked in threads_common.ready[priority] via scActive. Not implied by state == READY! */
	unsigned int callReturnable : 1;
	unsigned int respondAndRecv : 1;

	/* fastpath related */
	struct _thread_t *reply;     /* callers waiting for a response from us */
	struct _thread_t *replyNext; /* called->reply linkage */
	struct _thread_t *replyPrev;
	struct _thread_t *saveCtxIn; /* thread whose context our kernel stack holds */

	unsigned int sigmask;
	unsigned int sigpend;

	void *kstack;
	size_t kstacksz;
	char *ustack;

	hal_tls_t tls;

	/* for vfork/exec */
	void *parentkstack, *execkstack;
	void *execdata;

	cpu_context_t *context;
	cpu_context_t *fastpathExitCtx;
	cpu_context_t *longjmpctx;

	struct {
		/* the call this thread made, owned by xfer.h */
		struct {
			xferBuf_t ibl;
			xferBuf_t obl;
			u8 active;
			u8 ikind;
			u8 okind;
			u8 replied; /* the response header waits in reply for the switch to this thread */
			size_t isize;
			size_t osize;            /* the most a response may write back, then the size still to copy */
			void *odata;             /* caller's o.data */
			struct _thread_t *owner; /* set while the output still waits in owner's IPC buffer */
			void *bw;                /* the borrowed IPC buffer as the receiver sees it */
			vm_map_t *bmap;          /* NULL if nothing was mapped for the borrow */
		} call;

		/* IPC buffer */
		void *kw;
		void *w;
		page_t *p;
		size_t size;
		struct _thread_t *heldBy; /* the caller whose output is still in the buffer */

		u8 pulse;

		/* the response header, copied to msgPtr->o once in this thread's aspace */
		struct {
			unsigned char raw[MSG_RAW_SIZE];
			int err;
			size_t size;
		} reply;


		/* pointers in process' aspace */
		msg_t *msgPtr;
		msg_rid_t *ridPtr;
	} ipc;

	/* TODO: merge with ipc.flags? */
	int flags; /* atomic: IPC_PULSED and IPC_DELIVERED are set by other threads */
} thread_t;


static inline int proc_getTid(const thread_t *t)
{
	return t->idlinkage.id;
}


/* WARN: blockedKind readers bellow require threads_spinlock. */


static inline struct _lock_t *_thread_waitingOn(const thread_t *t)
{
	return (t->blockedKind == WAIT_LOCK) ? (struct _lock_t *)t->blockedOn : NULL;
}


static inline thread_t *_thread_waitingRecv(const thread_t *t)
{
	return (t->blockedKind == WAIT_RECV) ? (thread_t *)t->blockedOn : NULL;
}


static inline thread_t *_thread_called(const thread_t *t)
{
	return (t->blockedKind == WAIT_CALL) ? (thread_t *)t->blockedOn : NULL;
}


static inline void _thread_setBlocked(thread_t *t, unsigned int kind, void *obj)
{
	t->blockedOn = obj;
	t->blockedKind = kind;
}


static inline void _thread_clearBlocked(thread_t *t, unsigned int kind)
{
	if (t->blockedKind == kind) {
		t->blockedOn = NULL;
		t->blockedKind = WAIT_NOTHING;
	}
}


thread_t *proc_current(void);


thread_t *_proc_current(void);


void threads_canaryInit(thread_t *t, void *ustack);


int proc_threadCreate(process_t *process, startFn_t start, int *id, u8 priority, size_t kstacksz, void *stack, size_t stacksz, unsigned int sigmask, void *arg);


int proc_threadPriority(thread_t *t, int signedPriority);


__attribute__((noreturn)) void proc_threadEnd(void);


void proc_threadDestroy(thread_t *t);


void proc_threadsDestroy(thread_t **threads, const thread_t *except);


int proc_join(int tid, time_t timeout);


void proc_changeMap(process_t *proc, vm_map_t *map, vm_map_t *imap, pmap_t *pmap);


typedef void (*proc_threadsListCb_t)(void *arg, int i, threadinfo_t *info);


int proc_threadsIter(int n, proc_threadsListCb_t cb, void *arg);


int proc_threadsList(int n, threadinfo_t *info);


int proc_threadsOther(thread_t *t);


int proc_threadSleep(time_t us);


int proc_threadNanoSleep(time_t *sec, long int *nsec, int absolute);


int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp);


int proc_threadWaitInterruptible(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp);


int proc_threadWaitExclusive(thread_t **queue, time_t timeout);


int proc_threadWakeup(thread_t **queue);


void proc_threadWakeupYield(thread_t **queue);


int proc_threadBroadcast(thread_t **queue);


void proc_threadBroadcastYield(thread_t **queue);


int proc_schedInfo(int policy, sched_info_t *info);


int proc_schedGet(thread_t *t, sched_params_t *params);


int proc_schedSet(thread_t *t, int policy, sched_params_t *params);


thread_t *threads_findThread(int tid);


int proc_firstThreadTid(process_t *proc);


void threads_put(thread_t *thread);


time_t proc_uptime(void);


void proc_gettime(time_t *raw, time_t *offs);


int proc_settime(time_t offs);


__attribute__((noreturn)) void proc_longjmp(cpu_context_t *ctx);


void proc_threadsDump(u8 priority);


int _threads_init(vm_map_t *kmap, vm_object_t *kernel);


/*
 * The scheduler lock. Ports (ports.h) and SC donation (sc.h) are protected by
 * it as well, so everything below whose name starts with an underscore
 * assumes it is held.
 */
extern spinlock_t threads_spinlock;


/* Makes t run sc, or nothing at all if sc is NULL. Updates its effective
 * priority and its ready queue linkage. */
void _threads_setActiveSc(thread_t *t, sched_context_t *sc);


/* Monotonic time in us. */
time_t _proc_gettimeRaw(void);


/* Blocks t on a queue in the given state, without touching the ready queue. */
void _proc_threadEnqueueThread(thread_t *t, thread_t **queue, u8 state, time_t timeout, int interruptible);


/* Removes t from the queue it waits on. Does not wake it. */
void _proc_threadUnlink(thread_t *t);


/* Removes t from its queue and makes it runnable again. */
void _proc_threadDequeue(thread_t *t);


/* Wait time accounting for a thread that just blocked. */
void _threads_enqueued(thread_t *t);


/* Wakes a receiver waiting in msgRecv() with nothing to serve. */
void _wakeReceiver(thread_t *t);


int threads_sigpost(process_t *process, thread_t *thread, int sig);


int threads_sigsuspend(unsigned int mask);


void threads_setupUserReturn(void *retval, cpu_context_t *ctx);


__attribute__((noreturn)) void threads_halt(void);


cpu_context_t *_threads_switchTo(thread_t *dest);


cpu_context_t *_threads_getUserContext(thread_t *thread);


void _threads_setCurrent(thread_t *t);


void _threads_readyAdd(thread_t *t);


void _threads_ghostAdd(thread_t *t);


/* 1 if every thread of proc is dying or a ghost. */
int _proc_threadsGone(const process_t *proc);


#endif
