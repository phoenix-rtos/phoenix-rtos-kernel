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

#include "proc/proc.h"
#include "trace.h"


/* CTF event IDs */
enum {
	PERF_EVENT_LOCK_NAME = 0x01,
	PERF_EVENT_LOCK_SET = 0x02,
	PERF_EVENT_LOCK_CLEAR = 0x03,
	PERF_EVENT_INTERRUPT_ENTER = 0x04,
	PERF_EVENT_INTERRUPT_EXIT = 0x05,
	PERF_EVENT_THREAD_SCHEDULING = 0x06,
	PERF_EVENT_THREAD_PREEMPTED = 0x07,
	PERF_EVENT_THREAD_ENQUEUED = 0x08,
	PERF_EVENT_THREAD_WAKING = 0x09,
	PERF_EVENT_THREAD_CREATE = 0x0a,
	PERF_EVENT_THREAD_END = 0x0b,
	PERF_EVENT_SYSCALL_ENTER = 0x10,
	PERF_EVENT_SYSCALL_EXIT = 0x11,
	PERF_EVENT_SCHED_ENTER = 0x12,
	PERF_EVENT_SCHED_EXIT = 0x13,
};


extern void perf_traceEventsWrite(u8 event, const void *data, size_t sz);


/* Updates lock epoch counter. If lock hasn't been used in this trace epoch, emits LOCK_NAME event. */
extern void _perf_traceUpdateLockEpoch(lock_t *lock);


#define PERF_EVENT_BODY(event_id, ev, ...) \
	do { \
		if (perf_traceIsRunning() == 0) { \
			return; \
		} \
		__VA_ARGS__ perf_traceEventsWrite(event_id, &ev, sizeof(ev)); \
	} while (0)


/* assumes lock->spinlock is set */
static inline void _perf_traceEventsLockName(const lock_t *lock)
{
	struct {
		u32 lid;
		char name[16];
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_LOCK_NAME, ev, {
		ev.lid = (u32)lock;
		hal_strcpy(ev.name, lock->name);
	});
}


/* assumes lock->spinlock is set */
static inline void _perf_traceEventsLockSet(lock_t *lock, u32 tid)
{
	struct {
		u32 tid;
		u32 lid;
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_LOCK_SET, ev, {
		_perf_traceUpdateLockEpoch(lock);
		ev.tid = tid;
		ev.lid = (u32)lock;
	});
}


/* assumes lock->spinlock is set */
static inline void _perf_traceEventsLockClear(lock_t *lock, u32 tid)
{
	struct {
		u32 tid;
		u32 lid;
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_LOCK_CLEAR, ev, {
		_perf_traceUpdateLockEpoch(lock);
		ev.tid = tid;
		ev.lid = (u32)lock;
	});
}


static inline void perf_traceEventsInterruptEnter(u8 n)
{
	PERF_EVENT_BODY(PERF_EVENT_INTERRUPT_ENTER, n);
}


static inline void perf_traceEventsInterruptExit(u8 n)
{
	PERF_EVENT_BODY(PERF_EVENT_INTERRUPT_EXIT, n);
}


static inline void perf_traceEventsThreadScheduling(u32 tid)
{
	PERF_EVENT_BODY(PERF_EVENT_THREAD_SCHEDULING, tid);
}


static inline void perf_traceEventsThreadPreempted(u32 tid)
{
	PERF_EVENT_BODY(PERF_EVENT_THREAD_PREEMPTED, tid);
}


static inline void perf_traceEventsThreadEnqueued(u32 tid)
{
	PERF_EVENT_BODY(PERF_EVENT_THREAD_ENQUEUED, tid);
}


static inline void perf_traceEventsThreadWaking(u32 tid)
{
	PERF_EVENT_BODY(PERF_EVENT_THREAD_WAKING, tid);
}


static inline void perf_traceEventsThreadCreate(const thread_t *t)
{
	struct {
		u32 pid;
		u32 tid;
		u32 priority;
		char name[128];
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_THREAD_CREATE, ev, {
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
		u32 pid;
		u32 tid;
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_THREAD_END, ev, {
		ev.pid = process_getPid(t->process);
		ev.tid = proc_getTid(t);
	});
}


static inline void perf_traceEventsSyscallEnter(u8 n, u32 tid)
{
	struct {
		u8 val1;
		u32 val2;
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_SYSCALL_ENTER, ev, {
		ev.val1 = n;
		ev.val2 = tid;
	});
}


static inline void perf_traceEventsSyscallExit(u8 n, u32 tid)
{
	struct {
		u8 val1;
		u32 val2;
	} __attribute__((packed)) ev;

	PERF_EVENT_BODY(PERF_EVENT_SYSCALL_EXIT, ev, {
		ev.val1 = n;
		ev.val2 = tid;
	});
}


static inline void perf_traceEventsSchedEnter(u8 cpuId)
{
	PERF_EVENT_BODY(PERF_EVENT_SCHED_ENTER, cpuId);
}


static inline void perf_traceEventsSchedExit(u8 cpuId)
{
	PERF_EVENT_BODY(PERF_EVENT_SCHED_EXIT, cpuId);
}


#endif
