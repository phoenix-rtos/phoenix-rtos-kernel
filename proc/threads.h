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
#include "lib/lib.h"
#include "include/sysinfo.h"
#include "include/sched.h"
#include "process.h"
#include "lock.h"

#include <board_config.h>

#ifdef DEBUG_THREADS
#define LIB_ASSERT_THREADS(condition, fmt, ...) LIB_ASSERT_ALWAYS((condition), (fmt), ##__VA_ARGS__)
#else
#define LIB_ASSERT_THREADS(condition, fmt, ...)
#endif

#ifndef NPRIOS
#define NPRIOS 64U
#endif

_Static_assert(NPRIOS % 2U == 0U, "NPRIOS should be even");
_Static_assert(NPRIOS >= 16U, "NPRIOS should be >=16");

#define PRIO_OFFSET ((int)NPRIOS / 2)
#define MAX_PRIO    (PRIO_OFFSET - 1) /* Maximum priority value, of the lowest criticality (scheduled when no threads with p < MAX_PRIO are ready) */
#define MIN_PRIO    (-PRIO_OFFSET)    /* Minimum priority value, of the HIGHEST criticality (scheduled before MIN_PRIO + 1) */

#define TIME_T_MAX 0x7FFFFFFFFFFFFFFFLL /* LLONG_MAX */

typedef s8 priority_t;
_Static_assert(PRIO_OFFSET <= 128, "priority range must fit into priority_t");

#define MAX_TID        MAX_ID
#define THREAD_END     1U
#define THREAD_END_NOW 2U

/* Thread states */
#define READY 0U
#define SLEEP 1U
#define GHOST 2U

/* One tree per clock that has a timeline. PH_CLOCK_RELATIVE and the CPU-time clocks have none. */
#define CLOCK_IDX_MONOTONIC 0
#define CLOCK_IDX_REALTIME  1
#define CLOCK_IDX_COUNT     2


struct _ktimer_t;


/*
 * Run by the timer interrupt with the scheduler spinlock held. Returns the next absolute expiry
 * on the timer's own clock, or 0 to leave the timer disarmed.
 */
typedef time_t (*ktimerFire_t)(struct _ktimer_t *timer, time_t now);


/*
 * A deadline for something that is not a sleeping thread; threads keep their own linkage so
 * the far hotter sleep path stays free of the indirection.
 */
typedef struct _ktimer_t {
	rbnode_t linkage;              /* in the tree of a timeline clock */
	struct _ktimer_t *next, *prev; /* in the list of the CPU-time clocks */
	struct _ktimer_t *expnext;     /* transient, chains what one sweep found due */

	time_t expiry; /* absolute, in the domain of clock */
	ktimerFire_t fire;

	struct _process_t *targetProcess;
	struct _thread_t *targetThread;

	unsigned int id; /* orders timers that fall due at the same instant */
	unsigned int clock : 3;
	unsigned int armed : 1;
} ktimer_t;


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
	time_t wakeup;

	priority_t priorityBase;
	priority_t priority;

	unsigned int state : 2;
	unsigned int exit : 2;
	unsigned int interruptible : 1;
	unsigned int inKernel : 1;
	unsigned int wakeupClock : 1; /* CLOCK_IDX_* of the tree holding it while wakeup != 0 */

	unsigned int sigmask;
	unsigned int sigpend;

	void *kstack;
	size_t kstacksz;
	char *ustack;

	hal_tls_t tls;

	/* for vfork/exec */
	void *parentkstack, *execkstack;
	void *execdata;

	time_t readyTime;
	time_t maxWait;

	time_t startTime;
	time_t cpuTime;
	time_t lastTime;

	/*
	 * hint of cpu the thread was last scheduled onto, validated against current under the
	 * cpuSpinlock
	 */
	unsigned int cpuId;

	/* Exact split of cpuTime, charged at every user/kernel boundary */
	time_t sysTime;
	time_t userTime;

	cpu_context_t *context;
	cpu_context_t *longjmpctx;
} thread_t;


static inline int proc_getTid(const thread_t *t)
{
	return t->idlinkage.id;
}


thread_t *proc_current(void);


void threads_canaryInit(thread_t *t, void *ustack);


int proc_threadCreate(process_t *process, startFn_t start, int *id, priority_t priority, size_t kstacksz, void *stack, size_t stacksz, unsigned int sigmask, void *arg);


/* Sets the priority of t to val or retrieves the current priority of t when val == PH_GET_PRIO. Returns the current priority of t in *res if res != NULL. */
int proc_threadPriority(thread_t *t, int val, int *res);


__attribute__((noreturn)) void proc_threadEnd(void);


void proc_threadDestroy(thread_t *t);


void proc_threadsDestroy(thread_t **threads, const thread_t *except);


int proc_join(int tid, time_t timeout);


void proc_changeMap(process_t *proc, vm_map_t *map, vm_map_t *imap, pmap_t *pmap);


typedef void (*proc_threadsListFn_t)(void *arg, threadinfo_t *info);


void proc_threadsIter(unsigned int flags, proc_threadsListFn_t cb, void *arg);


int proc_threadsInfo(int tid, unsigned int flags, int n, threadinfo_t *info);


int proc_threadsOther(thread_t *t);


int proc_threadSleep(time_t us);


int proc_threadNanoSleep(time_t *sec, long int *nsec, int clockid, int absolute);


int proc_threadWait(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp);


int proc_threadWaitInterruptible(thread_t **queue, spinlock_t *spinlock, time_t timeout, spinlock_ctx_t *scp);


int proc_threadWaitExclusive(thread_t **queue, time_t abstime, int clockIdx);


int proc_threadWakeup(thread_t **queue);


void proc_threadWakeupYield(thread_t **queue);


int proc_threadBroadcast(thread_t **queue);


void proc_threadBroadcastYield(thread_t **queue);


int proc_schedInfo(int policy, sched_info_t *info);


int proc_schedGet(thread_t *t, sched_params_t *params);


int proc_schedSet(thread_t *t, int policy, sched_params_t *params);


int proc_cpuTime(const thread_t *t, int perThread, time_t *cpuTime, time_t *userTime, time_t *sysTime);


void proc_cpuTimeKernelEnter(thread_t *t);


void proc_cpuTimeKernelLeave(thread_t *t);


void *proc_cpuTimeIntrEnter(cpu_context_t *ctx);


void proc_cpuTimeIntrLeave(void *interrupted);


thread_t *threads_findThread(int tid);


int proc_firstThreadTid(process_t *proc);


void threads_put(thread_t *thread);


void proc_gettime(time_t *raw, time_t *offs);


int proc_settime(time_t offs);


/*
 * Resolves a timeout into an absolute deadline on the given clock's own timeline, along with the
 * index of the tree that timeline is kept in. A zero timeout means no deadline at all.
 */
int proc_clockTimeoutToAbsTime(int clock, time_t timeout, time_t *rabstime, int *rclockIdx);


/* For callers that already hold the scheduler spinlock - see threads_sigpost() */
int threads_sigpostLocked(process_t *process, thread_t *thread, int sig);


/* Whether sig has been raised for the process and not yet taken by any of its threads */
int threads_sigpending(const process_t *process, int sig);


void proc_ktimerArm(ktimer_t *timer, time_t expiry);


/* Takes the timer out of its clock. Safe to call on one that is not armed. */
void proc_ktimerDisarm(ktimer_t *timer);


/*
 * Reads the time left on the timer together with its armed state, so that the two cannot
 * disagree. Returns 1 when the timer is armed and 0 when it is not.
 */
int proc_ktimerRemaining(const ktimer_t *timer, time_t *remaining);


time_t proc_ktimerNow(const ktimer_t *timer);


__attribute__((noreturn)) void proc_longjmp(cpu_context_t *ctx);


void proc_threadsDump(priority_t priority);


int _threads_init(vm_map_t *kmap, vm_object_t *kernel);


int threads_sigpost(process_t *process, thread_t *thread, int sig);


int threads_sigsuspend(unsigned int mask);


int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


void threads_setupUserReturn(void *retval, cpu_context_t *ctx);


__attribute__((noreturn)) void threads_halt(void);


void threads_setSigmask(thread_t *thread, unsigned int sigmask);


int threads_setSigaction(int sig, void (*trampoline)(void), const struct sigaction *act, struct sigaction *old);


/* Requires guarantee that parent won't be reaped during execution. */
int proc_cloneSigactions(process_t *parent, process_t *child);


/* POSIX: signals ignored by the execve calling process should remain ignored */
void proc_resetExecSigactions(void);


#endif
