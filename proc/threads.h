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

#ifndef _PH_PROC_THREADS_H_
#define _PH_PROC_THREADS_H_

#include "hal/hal.h"
#include "include/msg.h"
#include "lib/lib.h"
#include "include/sysinfo.h"
#include "process.h"
#include "lock.h"

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


typedef struct _sched_context_t {
	struct _sched_context_t *next;
	struct _sched_context_t *prev;

	struct _sched_context_t *dnext;
	struct _sched_context_t *dprev;

	struct _thread_t *t;
	struct _thread_t *owner;
	struct _thread_t *donor;
	unsigned int priorityBase : 4;
	unsigned int priority : 4;

	time_t readyTime;
	time_t maxWait;

	time_t startTime;
	time_t cpuTime;
	time_t lastTime;
} sched_context_t;


#define MAX_PRIO 16


/* TODO: if not sufficient, implement some crazy heap */
typedef struct {
	struct _thread_t *pq[MAX_PRIO];
	int nonempty;
} prio_queue_t;


typedef struct {
	void *bvaddr;
	u64 boffs;
	page_t *bp;

	void *evaddr;
	u64 eoffs;
	page_t *ep;

	void *w;
	size_t size;
	vm_map_t *map;
} ipc_buf_layout_t;


#define IPC_IN_CALLER_BUF   (1 << 0)
#define IPC_OUT_CALLER_BUF  (1 << 1)
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

	struct _thread_t *tnext;
	struct _thread_t *tprev;

	int refs;
	struct _thread_t *blocking;
	struct _lock_t *waitingOn; /* lock this thread is blocked on (PI chain) */

	struct _thread_t **wait;
	time_t wakeup;

	sched_context_t *sc_own;     /* thread's base SC - never donated away */
	sched_context_t *sc_active;  /* SC currently being consumed (used by scheduler) */
	sched_context_t *sc_donated; /* SCs donated to the thread */

	unsigned int priorityBase : 4;
	unsigned int priority : 4;
	unsigned int state : 4;
	unsigned int interruptible : 1;
	unsigned int exit : 2;
	unsigned int passive : 1;

	/* fastpath related */
	struct _thread_t *reply;
	struct _thread_t *called;
	u8 fpCtxSet : 4;
	u8 callReturnable : 4;
	u8 saveCtxInReply : 4;
	u8 respondAndRecv : 4;

	/*
	 * REVISIT: during threads_destroy of a fastpath receiver we should remove
	 * ourselves out of addedTo's port queue so that it doesn't contain garbage
	 * but it's sad we need port-thread bound. Maybe there is a better way?
	 */
	struct _port_t *addedTo;

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
		ipc_buf_layout_t iil;
		ipc_buf_layout_t oil;

		/* extra buffer */
		void *kw;
		void *w;
		page_t *p;
		size_t size;
		u8 flags;

		void *bw; /* borrowed extra buf pointer */
		size_t bsize;

		u8 pulse;
		int err;

		msg_t msgDeferred;
		struct _thread_t *responseDeferredFrom;

		char msgbuf[MSG_RAW_SIZE];
		// size_t msglen;

		size_t ofs; /* buf ofs in kil */

		size_t esize;

		msg_rid_t *ridPtr;
		int ishmapped;
		int oshmapped;

		/* pointer to in process space */
		msg_t *msg;
	} utcb;

	int flags;

	/* Message buffer */
	struct _thread_t *mappedTo;
} thread_t;


static inline int proc_getTid(const thread_t *t)
{
	return t->idlinkage.id;
}


thread_t *proc_current(void);


void threads_canaryInit(thread_t *t, void *ustack);


int proc_threadCreate(process_t *process, startFn_t start, int *id, u8 priority, size_t kstacksz, void *stack, size_t stacksz, void *arg);


int proc_threadPriority(int signedPriority);


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


int proc_threadWakeup(thread_t **queue);


void proc_threadWakeupYield(thread_t **queue);


int proc_threadBroadcast(thread_t **queue);


void proc_threadBroadcastYield(thread_t **queue);


int proc_threadBroadcastPrio(prio_queue_t *queue);


void proc_threadPrioQueueInit(prio_queue_t *queue);


thread_t *threads_findThread(int tid);


void threads_put(thread_t *thread);


time_t proc_uptime(void);


void proc_gettime(time_t *raw, time_t *offs);


int proc_settime(time_t offs);


void proc_longjmp(cpu_context_t *ctx);


void proc_threadsDump(u8 priority);


int _threads_init(vm_map_t *kmap, vm_object_t *kernel);


int threads_sigpost(process_t *process, thread_t *thread, int sig);


int threads_sigsuspend(unsigned int mask);


void threads_setupUserReturn(void *retval, cpu_context_t *ctx);


extern int threads_getHighestPrio(int maxPrio);


extern void _threads_removeFromQueue(thread_t *t);


extern void threads_setState(u8 state);


extern void threads_releaseIpcBuffers(thread_t *thread);


#endif
