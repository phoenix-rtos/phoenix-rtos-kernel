/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Message profiling events
 *
 * Copyright 2026 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#ifndef _TRACE_MSG_H_
#define _TRACE_MSG_H_

#include "trace-events.h"

#define TSCS_COUNT 15                   /* (tid,syscall) + (isize, osize) + 15 tscs */
#define TSCS_SIZE  (1 + 1 + TSCS_COUNT) /* (tid,syscall) + (isize, osize) + 15 tscs */


int trace_emitMsgDesc(u8 step, u8 msgSyscall);

#define MSG_DESC_LEN 64


#if PERF_MSG

typedef struct {
	u64 tscs[TSCS_COUNT];
	u32 isize;
	u32 osize;
	u16 tid;
	u8 syscall;
	u8 ntscs;
} __attribute__((packed)) trace_eventMsgProfile_t;

static inline void trace_eventMsgProfileMeta(u8 syscall, u8 step, char *desc)
{
	struct {
		u8 syscall;
		u8 step;
		char desc[MSG_DESC_LEN];
	} __attribute__((packed)) ev;

	TRACE_MSG_BODY(TRACE_EVENT_MSG_PROFILE_META, ev, NULL, {
		ev.syscall = syscall;
		ev.step = step;
		hal_memcpy(ev.desc, desc, sizeof(ev.desc));
	});
}


static inline void trace_eventMsgProfile(u64 *data)
{
	if (trace_isRunning() == 0) {
		return;
	}
	trace_writeEvent(trace_channel_event, TRACE_EVENT_MSG_PROFILE, data, sizeof(u64) * TSCS_SIZE, NULL);
}


static inline void trace_msgProfileEnterFunc(u16 tid, u8 syscall, trace_eventMsgProfile_t *ev)
{
	hal_memset(ev, 0, sizeof(*ev));
	ev->tid = tid;
	ev->syscall = syscall;
}


static inline void trace_msgProfileExitFunc(size_t nextStep, u64 currTsc, trace_eventMsgProfile_t *ev)
{
	cycles_t cb;
	hal_cpuGetCycles(&cb);

	ev->tscs[nextStep - 1] = cb - currTsc;
	ev->ntscs = nextStep;

	TRACE_MSG_BODY(TRACE_EVENT_MSG_PROFILE, *ev, NULL);
}


static inline void trace_msgProfilePoint(u16 tid, size_t *nextStep, u64 *currTsc, trace_eventMsgProfile_t *ev, char *desc)
{
	cycles_t cb;
	hal_cpuGetCycles(&cb);

	size_t n = *nextStep;

	if (n > 0) {
		/* save previous profile point */
		ev->tscs[n - 1] = cb - *currTsc;
	}
	*nextStep += 1;

	if (trace_emitMsgDesc(n, ev->syscall) == 0) {
		trace_eventMsgProfileMeta(ev->syscall, n, desc);
	}

	hal_cpuGetCycles(&cb);
	*currTsc = cb;
}

#define TRACE_MSG_PROFILE_ENTER_FUNC(func) \
	trace_eventMsgProfile_t __ev; \
	size_t __nextStep = 0; \
	u64 __currTsc; \
	u16 __tid = proc_getTid(proc_current()); \
	trace_msgProfileEnterFunc(__tid, func, &__ev);
#define TRACE_MSG_PROFILE_SET_SIZES(i, o) \
	do { \
		(__ev).isize = i; \
		(__ev).osize = o; \
	} while (0);
#define TRACE_MSG_PROFILE_POINT(desc) trace_msgProfilePoint(__tid, &__nextStep, &__currTsc, &__ev, desc)
#define TRACE_MSG_PROFILE_EXIT_FUNC() trace_msgProfileExitFunc(__nextStep, __currTsc, &__ev)
#else
static inline void trace_eventMsgProfile(u64 *data)
{
}


static inline void trace_msgProfileExitFunc(u16 tid, u32 syscall, size_t *step, u64 currTsc, u64 *tscs)
{
}


static inline void trace_msgProfilePoint(u16 tid, size_t *step, u64 *currTsc, u64 *tscs)
{
}

#define TRACE_MSG_PROFILE_ENTER_FUNC(func)
#define TRACE_MSG_PROFILE_SET_SIZES(i, o)
#define TRACE_MSG_PROFILE_POINT(desc)
#define TRACE_MSG_PROFILE_EXIT_FUNC()
#endif


#endif
