/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * IPC profiling events
 *
 * Copyright 2026 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#ifndef _TRACE_IPC_H_
#define _TRACE_IPC_H_

#include "trace-events.h"

#define TSCS_SIZE 16 /* tid + 15 tscs */

/* clang-format off */
enum { trace_ipc_profile_send = 0, trace_ipc_profile_recv, trace_ipc_profile_respond, trace_ipc_profile_call, trace_ipc_profile_replyRecv };
/* clang-format on */


#if PERF_IPC
static inline void trace_eventMsgProfile(u64 *data)
{
	if (trace_isRunning() == 0) {
		return;
	}
	trace_writeEvent(trace_channel_event, TRACE_EVENT_MSG_PROFILE, data, sizeof(u64) * TSCS_SIZE, NULL);
}


static inline void trace_ipcProfileExitFunc(u16 tid, u32 syscall, size_t *step, u64 *currTsc, u64 *tscs)
{
	cycles_t cb;
	hal_cpuGetCycles(&cb);

	tscs[*step] = cb - *currTsc;
	((u32 *)tscs)[0] = tid;
	((u32 *)tscs)[1] = syscall;

	trace_eventMsgProfile(tscs);
}


static inline void trace_ipcProfilePoint(u16 tid, size_t *step, u64 *currTsc, u64 *tscs)
{
	cycles_t cb;
	size_t n = *step;
	hal_cpuGetCycles(&cb);

	if (n > 0) {
		tscs[n] = cb - *currTsc;
	}
	*currTsc = cb;
	*step += 1;
}

#define TRACE_IPC_PROFILE_EXIT_FUNC(tid, func, step, currTsc, tscs) trace_ipcProfileExitFunc(tid, func, step, currTsc, tscs)
#define TRACE_IPC_PROFILE_POINT(tid, step, currTsc, tscs)           trace_ipcProfilePoint(tid, step, currTsc, tscs)
#else
static inline void trace_eventMsgProfile(u64 *data)
{
}


static inline void trace_ipcProfileExitFunc(u16 tid, u32 syscall, size_t *step, u64 currTsc, u64 *tscs)
{
}


static inline void trace_ipcProfilePoint(u16 tid, size_t *step, u64 *currTsc, u64 *tscs)
{
}

#define TRACE_IPC_PROFILE_EXIT_FUNC(tid, func, step, currTsc, tscs)
#define TRACE_IPC_PROFILE_POINT(tid, step, currTsc, tscs)
#endif


#endif
