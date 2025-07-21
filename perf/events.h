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

#ifndef _PERF_EVENTS_H_
#define _PERF_EVENTS_H_

#include "include/perf.h"
#include "hal/types.h"
#include "proc/proc.h"
#include "trace.h"


/* CTF event IDs */
enum {
	PERF_EVENT_INTERRUPT_ENTER = 0x20,
	PERF_EVENT_INTERRUPT_EXIT = 0x21,
	PERF_EVENT_THREAD_SCHEDULING = 0x22,
	PERF_EVENT_THREAD_PREEMPTED = 0x23,
	PERF_EVENT_THREAD_ENQUEUED = 0x24,
	PERF_EVENT_THREAD_WAKING = 0x25,
	PERF_EVENT_THREAD_CREATE = 0x26,
	PERF_EVENT_THREAD_END = 0x27,
	PERF_EVENT_SYSCALL_ENTER = 0x28,
	PERF_EVENT_SYSCALL_EXIT = 0x29,
	PERF_EVENT_SCHED_ENTER = 0x2A,
	PERF_EVENT_SCHED_EXIT = 0x2B,
	PERF_EVENT_LOCK_NAME = 0x2C,
	PERF_EVENT_LOCK_SET_ENTER = 0x2D,
	PERF_EVENT_LOCK_SET_ACQUIRED = 0x2E,
	PERF_EVENT_LOCK_SET_EXIT = 0x2F,
	PERF_EVENT_LOCK_CLEAR = 0x30,
	PERF_EVENT_THREAD_PRIORITY = 0x40,
	PERF_EVENT_THREAD_INFOS = 0x41,
};


extern void perf_traceEventsWrite(u8 chan, u8 event, const void *data, size_t sz, u64 *ts);


/* Updates lock epoch counter. If lock hasn't been used in this trace epoch, emits LOCK_NAME event. */
extern void _perf_traceUpdateLockEpoch(lock_t *lock);


#define PERF_EVENT_BODY_CHAN(chan, event_id, ev, ts, ...) \
	do { \
		if (perf_traceIsRunning() == 0) { \
			return; \
		} \
		__VA_ARGS__ perf_traceEventsWrite(chan, event_id, &ev, sizeof(ev), ts); \
	} while (0)


#define PERF_META_BODY(event_id, ev, ts, ...)  PERF_EVENT_BODY_CHAN(perf_trace_channel_meta, event_id, ev, ts, __VA_ARGS__)
#define PERF_EVENT_BODY(event_id, ev, ts, ...) PERF_EVENT_BODY_CHAN(perf_trace_channel_event, event_id, ev, ts, __VA_ARGS__)


/* assumes lock->spinlock is set */
static inline void _perf_traceEventsLockName(const lock_t *lock)
{
	struct {
		u32 lid;
		char name[16];
	} __attribute__((packed)) ev;

	PERF_META_BODY(PERF_EVENT_LOCK_NAME, ev, NULL, {
		ev.lid = (ptr_t)lock;
		hal_strcpy(ev.name, lock->name);
	});
}


/* assumes lock->spinlock is set */
static inline void _perf_traceEventsLockSetEnter(lock_t *lock, u16 tid)
{
	struct {
		u16 tid;
		u32 lid;
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_LOCK_SET_ENTER, ev, NULL, {
		_perf_traceUpdateLockEpoch(lock);
		ev.tid = tid;
		ev.lid = (ptr_t)lock;
	});
}


/* assumes lock->spinlock is set */
static inline void _perf_traceEventsLockSetExit(lock_t *lock, u16 tid, int ret)
{
	struct {
		u16 tid;
		u32 lid;
	} __attribute__((packed)) ev;
	u64 ts = 0;

	PERF_EVENT_BODY(PERF_EVENT_LOCK_SET_EXIT, ev, &ts, {
		_perf_traceUpdateLockEpoch(lock);
		ev.tid = tid;
		ev.lid = (ptr_t)lock;
	});

	if (ret == EOK) {
		/* reuse lock_set_exit timestamp so that there is no gap between events */
		PERF_EVENT_BODY(PERF_EVENT_LOCK_SET_ACQUIRED, ev, &ts, {
			/* epoch already updated */
			ev.tid = tid;
			ev.lid = (ptr_t)lock;
		});
	}
}


/* assumes lock->spinlock is set */
static inline void _perf_traceEventsLockClear(lock_t *lock, u16 tid)
{
	struct {
		u16 tid;
		u32 lid;
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_LOCK_CLEAR, ev, NULL, {
		_perf_traceUpdateLockEpoch(lock);
		ev.tid = tid;
		ev.lid = (ptr_t)lock;
	});
}


static inline void perf_traceEventsInterruptEnter(u8 n)
{
	PERF_EVENT_BODY(PERF_EVENT_INTERRUPT_ENTER, n, NULL);
}


static inline void perf_traceEventsInterruptExit(u8 n)
{
	PERF_EVENT_BODY(PERF_EVENT_INTERRUPT_EXIT, n, NULL);
}


static inline void perf_traceEventsThreadScheduling(u16 tid)
{
	PERF_EVENT_BODY(PERF_EVENT_THREAD_SCHEDULING, tid, NULL);
}


static inline void perf_traceEventsThreadPreempted(u16 tid)
{
	PERF_EVENT_BODY(PERF_EVENT_THREAD_PREEMPTED, tid, NULL);
}


static inline void perf_traceEventsThreadEnqueued(u16 tid)
{
	PERF_EVENT_BODY(PERF_EVENT_THREAD_ENQUEUED, tid, NULL);
}


static inline void perf_traceEventsThreadWaking(u16 tid)
{
	PERF_EVENT_BODY(PERF_EVENT_THREAD_WAKING, tid, NULL);
}


static inline void perf_traceEventsThreadCreate(const thread_t *t)
{
	struct {
		u16 pid;
		u16 tid;
		u8 priority;
		char name[128];
	} __attribute__((packed)) ev;

	PERF_META_BODY(PERF_EVENT_THREAD_CREATE, ev, NULL, {
		ev.tid = proc_getTid(t);
		ev.priority = t->priority;

		if (t->process != NULL) {
			ev.pid = process_getPid(t->process);
			process_getName(t->process, ev.name, sizeof(ev.name));
		}
		else {
			ev.pid = 0;
			hal_memcpy(ev.name, "[idle]", sizeof("[idle]"));
		}
	});
}


static inline void perf_traceEventsThreadEnd(const thread_t *t)
{
	struct {
		u16 pid;
		u16 tid;
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_THREAD_END, ev, NULL, {
		ev.pid = process_getPid(t->process);
		ev.tid = proc_getTid(t);
	});
}


static inline void perf_traceEventsSyscallEnter(u8 n, u16 tid)
{
	struct {
		u8 n;
		u16 tid;
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_SYSCALL_ENTER, ev, NULL, {
		ev.n = n;
		ev.tid = tid;
	});
}


static inline void perf_traceEventsSyscallExit(u8 n, u16 tid)
{
	struct {
		u8 n;
		u16 tid;
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_SYSCALL_EXIT, ev, NULL, {
		ev.n = n;
		ev.tid = tid;
	});
}


static inline void perf_traceEventsSchedEnter(u8 cpuId)
{
	PERF_EVENT_BODY(PERF_EVENT_SCHED_ENTER, cpuId, NULL);
}


static inline void perf_traceEventsSchedExit(u8 cpuId)
{
	PERF_EVENT_BODY(PERF_EVENT_SCHED_EXIT, cpuId, NULL);
}


static inline void perf_traceEventsThreadPriority(u16 tid, u8 priority)
{
	struct {
		u16 tid;
		u8 priority;
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_THREAD_PRIORITY, ev, NULL, {
		ev.tid = tid;
		ev.priority = priority;
	});
}


#endif
