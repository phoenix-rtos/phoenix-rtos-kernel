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
#include "include/syscalls.h"
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
	TRACE_EVENT_SCHED_ENTER = 0x2a,
	TRACE_EVENT_SCHED_EXIT = 0x2b,
	TRACE_EVENT_LOCK_NAME = 0x2c,
	TRACE_EVENT_LOCK_SET_ENTER = 0x2d,
	TRACE_EVENT_LOCK_SET_ACQUIRED = 0x2e,
	TRACE_EVENT_LOCK_SET_EXIT = 0x2f,
	TRACE_EVENT_LOCK_CLEAR = 0x30,
	TRACE_EVENT_THREAD_PRIORITY = 0x31,
	TRACE_EVENT_PROCESS_KILL = 0x32,
};


void trace_writeEvent(u8 cpuChan, u8 event, const void *data, size_t sz, u32 *ts);


/*
 * Updates lock epoch counter. If lock hasn't been used in this trace epoch,
 * emits LOCK_NAME event.
 */
void _trace_updateLockEpoch(lock_t *lock);


#define TRACE_EVENT_BODY_CHAN(chan, event_id, ev, ts, ...) \
	do { \
		if (trace_isRunning() == 0) { \
			return; \
		} \
		__VA_ARGS__ trace_writeEvent((chan), (event_id), &(ev), sizeof(ev), (ts)); \
	} while (0)


/*
 * NOTE: The ev structure passed to PERF_{META,EVENT}_BODY must match the
 * field struct declared in the tsdl/metadata for a given event_id.
 */
#define TRACE_META_BODY(event_id, ev, ts, ...)  TRACE_EVENT_BODY_CHAN((u8)(trace_channel_meta), (event_id), (ev), (ts), __VA_ARGS__)
#define TRACE_EVENT_BODY(event_id, ev, ts, ...) TRACE_EVENT_BODY_CHAN((u8)(trace_channel_event), (event_id), (ev), (ts), __VA_ARGS__)


/* assumes lock->spinlock is set */
static inline void _trace_eventLockName(const lock_t *lock)
{
	struct {
		u32 lid;
		char name[16];
	} __attribute__((packed)) ev;

	TRACE_META_BODY(TRACE_EVENT_LOCK_NAME, ev, NULL, {
		/*
		 * It's safe to downcast kernel space lock address on 64-bit MMU targets to 32-bit.
		 * Kernel space is contiguous on MMU and its address range doesn't exceed 32-bits,
		 * so if we crop 64-bit lock_t address to 32-bit it will stay unique.
		 * On NOMMU it isn't necessarily contiguous, but sane NOMMU targets are 32-bit,
		 * so there's no downcast.
		 */
		ev.lid = (u32)(ptr_t)lock;
		(void)hal_strcpy(ev.name, lock->name);
	});
}


/* assumes lock->spinlock is set */
static inline void _trace_eventLockSetEnter(lock_t *lock, int tid)
{
	struct {
		u16 tid;
		u32 lid;
	} __attribute__((packed)) ev;

	TRACE_EVENT_BODY(TRACE_EVENT_LOCK_SET_ENTER, ev, NULL, {
		_trace_updateLockEpoch(lock);
		ev.tid = (u16)tid;
		ev.lid = (u32)(ptr_t)lock;
	});
}


/* assumes lock->spinlock is set */
static inline void _trace_eventLockSetExit(lock_t *lock, int tid, int ret)
{
	struct {
		u16 tid;
		u32 lid;
	} __attribute__((packed)) ev;
	u32 ts = 0;

	TRACE_EVENT_BODY(TRACE_EVENT_LOCK_SET_EXIT, ev, &ts, {
		_trace_updateLockEpoch(lock);
		ev.tid = (u16)tid;
		ev.lid = (u32)(ptr_t)lock;
	});

	if (ret == EOK) {
		/* reuse lock_set_exit timestamp so that there is no gap between events */
		TRACE_EVENT_BODY(TRACE_EVENT_LOCK_SET_ACQUIRED, ev, &ts, {
			/* epoch already updated */
			ev.tid = (u16)tid;
			ev.lid = (u32)(ptr_t)lock;
		});
	}
}


/* assumes lock->spinlock is set */
static inline void _trace_eventLockClear(lock_t *lock, int tid)
{
	struct {
		u16 tid;
		u32 lid;
	} __attribute__((packed)) ev;

	TRACE_EVENT_BODY(TRACE_EVENT_LOCK_CLEAR, ev, NULL, {
		_trace_updateLockEpoch(lock);
		ev.tid = (u16)tid;
		ev.lid = (u32)(ptr_t)lock;
	});
}


/* parasoft-suppress-next-line MISRAC2012-RULE_2_1-h "False positive, function already marked as unused" */
MAYBE_UNUSED static inline void trace_eventInterruptEnter(unsigned int n)
{
	u8 ev = 0;
	TRACE_EVENT_BODY(TRACE_EVENT_INTERRUPT_ENTER, ev, NULL, { ev = (u8)n; });
}


/* parasoft-suppress-next-line MISRAC2012-RULE_2_1-h "False positive, function already marked as unused" */
MAYBE_UNUSED static inline void trace_eventInterruptExit(unsigned int n)
{
	u8 ev = 0;
	TRACE_EVENT_BODY(TRACE_EVENT_INTERRUPT_EXIT, ev, NULL, { ev = (u8)n; });
}


static inline void trace_eventThreadScheduling(int tid)
{
	u16 ev = 0;
	TRACE_EVENT_BODY(TRACE_EVENT_THREAD_SCHEDULING, ev, NULL, { ev = (u16)tid; });
}


static inline void trace_eventThreadPreempted(int tid)
{
	u16 ev = 0;
	TRACE_EVENT_BODY(TRACE_EVENT_THREAD_PREEMPTED, ev, NULL, { ev = (u16)tid; });
}


static inline void trace_eventThreadEnqueued(int tid)
{
	u16 ev = 0;
	TRACE_EVENT_BODY(TRACE_EVENT_THREAD_ENQUEUED, ev, NULL, { ev = (u16)tid; });
}


static inline void trace_eventThreadWaking(int tid)
{
	u16 ev = 0;
	TRACE_EVENT_BODY(TRACE_EVENT_THREAD_WAKING, ev, NULL, { ev = (u16)tid; });
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
		ev.tid = (u16)proc_getTid(t);
		ev.priority = (u8)t->priority;

		if (t->process != NULL) {
			ev.pid = (u16)process_getPid(t->process);
			process_getName(t->process, ev.name, sizeof(ev.name));
		}
		else {
			ev.pid = 0;
			hal_memcpy(ev.name, "[kernel]", sizeof("[kernel]"));
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
		ev.pid = (u16)process_getPid(t->process);
		ev.tid = (u16)proc_getTid(t);
	});
}


static inline void trace_eventSyscallEnter(int n, int tid)
{
	struct {
		u8 n;
		u16 tid;
	} __attribute__((packed)) ev;

	_Static_assert((u64)syscall_count <= (1UL << 8U * sizeof(u8)) - 1UL, "u8 is too small for syscall ID");
	TRACE_EVENT_BODY(TRACE_EVENT_SYSCALL_ENTER, ev, NULL, {
		ev.n = (u8)n;
		ev.tid = (u16)tid;
	});
}


static inline void trace_eventSyscallExit(int n, int tid)
{
	struct {
		u8 n;
		u16 tid;
	} __attribute__((packed)) ev;

	TRACE_EVENT_BODY(TRACE_EVENT_SYSCALL_EXIT, ev, NULL, {
		ev.n = (u8)n;
		ev.tid = (u16)tid;
	});
}


static inline void trace_eventSchedEnter(unsigned int cpuId)
{
	u8 ev = 0;
	TRACE_EVENT_BODY(TRACE_EVENT_SCHED_ENTER, ev, NULL, { ev = (u8)cpuId; });
}


static inline void trace_eventSchedExit(unsigned int cpuId)
{
	u8 ev = 0;
	TRACE_EVENT_BODY(TRACE_EVENT_SCHED_EXIT, ev, NULL, { ev = (u8)cpuId; });
}


static inline void trace_eventThreadPriority(int tid, u8 priority)
{
	struct {
		u16 tid;
		u8 priority;
	} __attribute__((packed)) ev;

	TRACE_EVENT_BODY(TRACE_EVENT_THREAD_PRIORITY, ev, NULL, {
		ev.tid = (u16)tid;
		ev.priority = priority;
	});
}


static inline void trace_eventProcessKill(const process_t *p)
{
	u16 pid;

	TRACE_EVENT_BODY(TRACE_EVENT_PROCESS_KILL, pid, NULL, {
		pid = (u16)process_getPid(p);
	});
}


#endif
