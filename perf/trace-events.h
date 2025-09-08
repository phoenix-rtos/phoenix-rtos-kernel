/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance analysis subsystem - kernel events to Common Trace Format writer
 *
 * Event stream conforms to the metadata stream located under
 * perf/tsdl/metadata.
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#ifndef _TRACE_EVENTS_H_
#define _TRACE_EVENTS_H_

#include "include/perf.h"
#include "hal/types.h"
#include "proc/proc.h"
#include "trace.h"


/* NOTE: must mirror tsdl/metadata event IDs. */
enum {
	TRACE_EVENT_INTERRUPT_ENTER = 0x20,
	TRACE_EVENT_INTERRUPT_EXIT = 0x21,
	TRACE_EVENT_THREAD_SCHEDULING = 0x22,
	TRACE_EVENT_THREAD_PREEMPTED = 0x23,
	TRACE_EVENT_THREAD_ENQUEUED = 0x24,
	TRACE_EVENT_THREAD_WAKING = 0x25,
	TRACE_EVENT_THREAD_CREATE = 0x26,
	TRACE_EVENT_THREAD_END = 0x27,
	TRACE_EVENT_SYSCALL_ENTER = 0x28,
	TRACE_EVENT_SYSCALL_EXIT = 0x29,
	TRACE_EVENT_SCHED_ENTER = 0x2A,
	TRACE_EVENT_SCHED_EXIT = 0x2B,
	TRACE_EVENT_LOCK_NAME = 0x2C,
	TRACE_EVENT_LOCK_SET_ENTER = 0x2D,
	TRACE_EVENT_LOCK_SET_ACQUIRED = 0x2E,
	TRACE_EVENT_LOCK_SET_EXIT = 0x2F,
	TRACE_EVENT_LOCK_CLEAR = 0x30,
	TRACE_EVENT_THREAD_PRIORITY = 0x31,
	TRACE_EVENT_PROCESS_KLL = 0x32,
	TRACE_EVENT_IPC_RTT_ENTER = 0x40,
	TRACE_EVENT_IPC_RTT_EXIT = 0x41,
};


extern void trace_writeEvent(u8 cpuChan, u8 event, const void *data, size_t sz, u32 *ts);


/*
 * Updates lock epoch counter. If lock hasn't been used in this trace epoch,
 * emits LOCK_NAME event.
 */
extern void _trace_updateLockEpoch(lock_t *lock);


#define TRACE_EVENT_BODY_CHAN(chan, event_id, ev, ts, ...) \
	do { \
		if (trace_isRunning() == 0) { \
			return; \
		} \
		__VA_ARGS__ trace_writeEvent(chan, event_id, &ev, sizeof(ev), ts); \
	} while (0)


#define PERF_IPC 1

/* clang-format off */
#define NO_EVENT(ev, ts) do { (void)ev; (void)ts; } while (0)
/* clang-format on */

/*
 * NOTE: The ev structure passed to PERF_{META,EVENT}_BODY must match the
 * field struct declared in the tsdl/metadata for a given event_id.
 */
#if !PERF_IPC
#define TRACE_META_BODY(event_id, ev, ts, ...)  TRACE_EVENT_BODY_CHAN(trace_channel_meta, event_id, ev, ts, __VA_ARGS__)
#define TRACE_EVENT_BODY(event_id, ev, ts, ...) TRACE_EVENT_BODY_CHAN(trace_channel_event, event_id, ev, ts, __VA_ARGS__)
#define TRACE_IPC_BODY(event_id, ev, ts, ...)   NO_EVENT(ev, ts)
#else
#define TRACE_META_BODY(event_id, ev, ts, ...)  NO_EVENT(ev, ts)
#define TRACE_EVENT_BODY(event_id, ev, ts, ...) NO_EVENT(ev, ts)
#define TRACE_IPC_BODY(event_id, ev, ts, ...)   TRACE_EVENT_BODY_CHAN(trace_channel_event, event_id, ev, ts, __VA_ARGS__)
#endif


/* assumes lock->spinlock is set */
static inline void _trace_eventLockName(const lock_t *lock)
{
	struct {
		u32 lid;
		char name[16];
	} __attribute__((packed)) ev;

	TRACE_META_BODY(TRACE_EVENT_LOCK_NAME, ev, NULL, {
		ev.lid = (ptr_t)lock;
		hal_strcpy(ev.name, lock->name);
	});
}


/* assumes lock->spinlock is set */
static inline void _trace_eventLockSetEnter(lock_t *lock, u16 tid)
{
	struct {
		u16 tid;
		u32 lid;
	} __attribute__((packed)) ev;

	TRACE_EVENT_BODY(TRACE_EVENT_LOCK_SET_ENTER, ev, NULL, {
		_trace_updateLockEpoch(lock);
		ev.tid = tid;
		ev.lid = (ptr_t)lock;
	});
}


/* assumes lock->spinlock is set */
static inline void _trace_eventLockSetExit(lock_t *lock, u16 tid, int ret)
{
	struct {
		u16 tid;
		u32 lid;
	} __attribute__((packed)) ev;
	u32 ts = 0;

	TRACE_EVENT_BODY(TRACE_EVENT_LOCK_SET_EXIT, ev, &ts, {
		_trace_updateLockEpoch(lock);
		ev.tid = tid;
		ev.lid = (ptr_t)lock;
	});

	if (ret == EOK) {
		/* reuse lock_set_exit timestamp so that there is no gap between events */
		TRACE_EVENT_BODY(TRACE_EVENT_LOCK_SET_ACQUIRED, ev, &ts, {
			/* epoch already updated */
			ev.tid = tid;
			ev.lid = (ptr_t)lock;
		});
	}
}


/* assumes lock->spinlock is set */
static inline void _trace_eventLockClear(lock_t *lock, u16 tid)
{
	struct {
		u16 tid;
		u32 lid;
	} __attribute__((packed)) ev;

	TRACE_EVENT_BODY(TRACE_EVENT_LOCK_CLEAR, ev, NULL, {
		_trace_updateLockEpoch(lock);
		ev.tid = tid;
		ev.lid = (ptr_t)lock;
	});
}


static inline void trace_eventInterruptEnter(u8 n)
{
	TRACE_EVENT_BODY(TRACE_EVENT_INTERRUPT_ENTER, n, NULL);
}


static inline void trace_eventInterruptExit(u8 n)
{
	TRACE_EVENT_BODY(TRACE_EVENT_INTERRUPT_EXIT, n, NULL);
}


static inline void trace_eventThreadScheduling(u16 tid)
{
	TRACE_EVENT_BODY(TRACE_EVENT_THREAD_SCHEDULING, tid, NULL);
}


static inline void trace_eventThreadPreempted(u16 tid)
{
	TRACE_EVENT_BODY(TRACE_EVENT_THREAD_PREEMPTED, tid, NULL);
}


static inline void trace_eventThreadEnqueued(u16 tid)
{
	TRACE_EVENT_BODY(TRACE_EVENT_THREAD_ENQUEUED, tid, NULL);
}


static inline void trace_eventThreadWaking(u16 tid)
{
	TRACE_EVENT_BODY(TRACE_EVENT_THREAD_WAKING, tid, NULL);
}


static inline void trace_eventThreadCreate(const thread_t *t)
{
	struct {
		u16 pid;
		u16 tid;
		u8 priority;
		char name[128];
	} __attribute__((packed)) ev;

	TRACE_META_BODY(TRACE_EVENT_THREAD_CREATE, ev, NULL, {
		ev.tid = proc_getTid(t);
		ev.priority = t->sched->priority;

		if (t->process != NULL) {
			ev.pid = process_getPid(t->process);
			process_getName(t->process, ev.name, sizeof(ev.name));
		}
		else {
			ev.pid = 0;
			hal_memcpy(ev.name, "[kthr]", sizeof("[kthr]"));
		}
	});
}


static inline void trace_eventThreadEnd(const thread_t *t)
{
	struct {
		u16 pid;
		u16 tid;
	} __attribute__((packed)) ev;

	TRACE_EVENT_BODY(TRACE_EVENT_THREAD_END, ev, NULL, {
		ev.pid = process_getPid(t->process);
		ev.tid = proc_getTid(t);
	});
}


static inline void trace_eventSyscallEnter(u8 n, u16 tid)
{
	struct {
		u8 n;
		u16 tid;
	} __attribute__((packed)) ev;

	TRACE_EVENT_BODY(TRACE_EVENT_SYSCALL_ENTER, ev, NULL, {
		ev.n = n;
		ev.tid = tid;
	});
}


static inline void trace_eventSyscallExit(u8 n, u16 tid)
{
	struct {
		u8 n;
		u16 tid;
	} __attribute__((packed)) ev;

	TRACE_EVENT_BODY(TRACE_EVENT_SYSCALL_EXIT, ev, NULL, {
		ev.n = n;
		ev.tid = tid;
	});
}


static inline void trace_eventSchedEnter(u8 cpuId)
{
	TRACE_EVENT_BODY(TRACE_EVENT_SCHED_ENTER, cpuId, NULL);
}


static inline void trace_eventSchedExit(u8 cpuId)
{
	TRACE_EVENT_BODY(TRACE_EVENT_SCHED_EXIT, cpuId, NULL);
}


static inline void trace_eventThreadPriority(u16 tid, u8 priority)
{
	struct {
		u16 tid;
		u8 priority;
	} __attribute__((packed)) ev;

	TRACE_EVENT_BODY(TRACE_EVENT_THREAD_PRIORITY, ev, NULL, {
		ev.tid = tid;
		ev.priority = priority;
	});
}


static inline void trace_eventProcessKill(const process_t *p)
{
	u16 pid;

	TRACE_EVENT_BODY(TRACE_EVENT_PROCESS_KLL, pid, NULL, {
		pid = process_getPid(p);
	});
}


static inline void trace_eventIPCEnter(void)
{
	u64 tsc;

	TRACE_IPC_BODY(TRACE_EVENT_IPC_RTT_ENTER, tsc, NULL, {
		hal_cpuGetCycles(&tsc);
	});
}


static inline void trace_eventIPCExit(void)
{
	u64 tsc;

	TRACE_IPC_BODY(TRACE_EVENT_IPC_RTT_EXIT, tsc, NULL, {
		hal_cpuGetCycles(&tsc);
	});
}


#endif
