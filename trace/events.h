/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance tracing subsystem - kernel events
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#ifndef _TRACE_EVENTS_H_
#define _TRACE_EVENTS_H_


#include "proc/proc.h"


/* CTF event IDs */
enum {
	TRACE_EVENT_LOCK_SET = 0x10,
	TRACE_EVENT_LOCK_CLEAR = 0x11,
	TRACE_EVENT_INTERRUPT_ENTER = 0x20,
	TRACE_EVENT_INTERRUPT_EXIT = 0x21,
};


extern time_t _trace_gettimeRaw(void);


extern void trace_eventsU8(u8 event, u8 val);


extern void trace_eventsU32Str(u8 event, u32 val, const char *str);


/* TODO: spinlocks? */


static inline void trace_eventsLockSet(lock_t *lock, int tid)
{
	trace_eventsU32Str(TRACE_EVENT_LOCK_SET, tid, lock->name);
}


static inline void trace_eventsLockClear(lock_t *lock, int tid)
{
	trace_eventsU32Str(TRACE_EVENT_LOCK_CLEAR, tid, lock->name);
}


static inline void trace_eventsInterruptEnter(int n)
{
	trace_eventsU8(TRACE_EVENT_INTERRUPT_ENTER, n);
}


static inline void trace_eventsInterruptExit(int n)
{
	trace_eventsU8(TRACE_EVENT_INTERRUPT_EXIT, n);
}


#endif
