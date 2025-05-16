/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance analysis subsystem - CTF backend
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */


#include "include/perf.h"
#include "buffer.h"
#include "events.h"
#include "trace.h"


static struct {
	/*
	 * Treat `gather` as atomic to reduce overhead on the kernel when the tracing is disabled
	 * - there is only one writer at a time (perf_trace{Start,Finish}()) and multiple readers
	 * (trace events doing perf_traceIsRunning()). Due to eventual consistency in the readers
	 * we may lose some events, but we may lose them anyway as the invocation of
	 * perf_traceStart() naturally races with kernel events occurring in the meantime.
	 *
	 * `running` is an always consistent copy of `gather` used for proper synchronization of
	 * tracing state.
	 */
	volatile int gather;

	spinlock_t spinlock;

	/* guarded by spinlock */
	int running;
	int stopped;
	u64 prev;
	int epoch;
	unsigned flags;
	u8 errorFlags;
	u64 eventDelayTimestamp;
} trace_common;


#define TRACE_NON_MONOTONICITY (1 << 1)
#define TRACE_EVENT_DELAYED    (1 << 2)
#define TRACE_BUFFER_WRITE_ERR (1 << 3)


static u32 _perf_traceGettimeRaw(void)
{
	u32 now = hal_timerGetUs();

	while (now < trace_common.prev) {
		trace_common.errorFlags |= TRACE_NON_MONOTONICITY;
		now = hal_timerGetUs();
	}

	trace_common.prev = now;

	return now;
}


static void _perf_traceEventsWrite(u8 cpuChan, u8 event, const void *data, size_t sz, u32 *ts)
{
	u32 eventTs;
	int ret, try = 0;
	int eventSz = sizeof(eventTs) + sizeof(event) + sz;
	int avail;
	u8 chan = cpuChan + hal_cpuGetID() * perf_trace_channel_count;

	struct {
		u32 ts;
		u8 eventId;
	} __attribute__((packed)) ev;

	if (ts == NULL || *ts == 0) {
		eventTs = _perf_traceGettimeRaw();
		if (ts != NULL) {
			*ts = eventTs;
		}
	}
	else {
		/* use timestamp provided by the caller */
		eventTs = *ts;
	}

	do {
		avail = _trace_bufferAvail(chan);
		if (avail < eventSz) {
			if ((trace_common.flags & PERF_TRACE_FLAG_ROLLING) != 0) {
				_trace_bufferDiscard(chan, eventSz - avail);
			}
			else {
				try = _trace_bufferWaitUntilAvail(chan, eventSz);
			}
		}

		ev.ts = eventTs;
		ev.eventId = event;
		ret = _trace_bufferWrite(chan, &ev, sizeof(ev));
		if (ret != sizeof(ev)) {
			break;
		}

		ret = _trace_bufferWrite(chan, data, sz);
		if (ret != sz) {
			break;
		}
	} while (0);

	if (ret < 0) {
		trace_common.errorFlags |= TRACE_BUFFER_WRITE_ERR;
	}
	else if ((trace_common.errorFlags & TRACE_EVENT_DELAYED) == 0 && try > 0) {
		/*
		 * Record first occurrence of event delay to caution the user about possible
		 * loss of timestamp precision. This may happen if e.g. the buffer is implemented as RTT
		 * and the receiver (debug probe) can't keep up with the event generation rate
		 */
		trace_common.errorFlags |= TRACE_EVENT_DELAYED;
		trace_common.eventDelayTimestamp = eventTs;
	}
}


/* WARN: should be callable from interrupt handler */
void perf_traceEventsWrite(u8 cpuChan, u8 event, const void *data, size_t sz, u32 *ts)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	if (trace_common.running != 0) {
		_perf_traceEventsWrite(cpuChan, event, data, sz, ts);
	}
	hal_spinlockClear(&trace_common.spinlock, &sc);
}


void _perf_traceUpdateLockEpoch(lock_t *lock)
{
	int prev = _proc_lockSetTraceEpoch(lock, trace_common.epoch);

	if (prev != trace_common.epoch) {
		_perf_traceEventsLockName(lock);
	}
}


/* WARN: eventually consistent */
int perf_traceIsRunning(void)
{
	return trace_common.gather;
}


static void _perf_traceEmitThreadsCb(void *arg, int i, threadinfo_t *tinfo)
{
	struct {
		u16 pid;
		u16 tid;
		u8 priority;
		char name[128];
	} __attribute__((packed)) ev;

	ev.tid = tinfo->tid;
	ev.priority = tinfo->priority;
	ev.pid = tinfo->pid;

	hal_memcpy(ev.name, tinfo->name, sizeof(tinfo->name));

	_perf_traceEventsWrite(perf_trace_channel_meta, PERF_EVENT_THREAD_CREATE, &ev, sizeof(ev), NULL);
}


static void _perf_emitThreadinfo(void)
{
	(void)proc_threadsIter(0xFFFF, _perf_traceEmitThreadsCb, NULL);
}


static void _perf_enableTracing(int enable)
{
	int val = !!enable;
	trace_common.running = val;
	trace_common.gather = val;
	_hal_interruptsTrace(val);
}


static int perf_traceChannelCount(void)
{
	return hal_cpuGetCount() * perf_trace_channel_count;
}


int perf_traceStart(unsigned flags)
{
	spinlock_ctx_t sc;
	int ret;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	do {
		if (trace_common.running != 0) {
			ret = -EINVAL;
			break;
		}

		ret = _trace_bufferStart();
		if (ret < 0) {
			break;
		}

		trace_common.flags = flags;

		_perf_emitThreadinfo();
		trace_common.errorFlags = 0;
		trace_common.epoch++;
		_perf_enableTracing(1);

		ret = perf_traceChannelCount();
	} while (0);
	hal_spinlockClear(&trace_common.spinlock, &sc);

	return ret;
}


int perf_traceRead(u8 chan, void *buf, size_t bufsz)
{
	spinlock_ctx_t sc;
	int ret;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	if (trace_common.running != 0 || trace_common.stopped != 0) {
		ret = _trace_bufferRead(chan, buf, bufsz);
	}
	else {
		ret = -EINVAL;
	}
	hal_spinlockClear(&trace_common.spinlock, &sc);

	return ret;
}


int perf_traceStop(void)
{
	int ret = EOK;
	spinlock_ctx_t sc;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	if (trace_common.stopped == 0 && trace_common.running != 0) {
		_perf_enableTracing(0);
		trace_common.stopped = 1;
		ret = perf_traceChannelCount();
	}
	else {
		ret = -EINVAL;
	}
	hal_spinlockClear(&trace_common.spinlock, &sc);

	return ret;
}


int perf_traceFinish(void)
{
	spinlock_ctx_t sc;
	int ret = EOK;
	u8 errorFlags = 0;
	u64 eventDelayTimestamp = 0;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	if (trace_common.running != 0 || trace_common.stopped != 0) {
		_perf_enableTracing(0);
		trace_common.stopped = 0;
		errorFlags = trace_common.errorFlags;
		eventDelayTimestamp = trace_common.eventDelayTimestamp;
	}
	else {
		ret = -EINVAL;
	}
	hal_spinlockClear(&trace_common.spinlock, &sc);

	if ((errorFlags & TRACE_NON_MONOTONICITY) != 0) {
		lib_printf("kernel (%s:%d): timer non-monotonicity detected during event gathering\n", __func__, __LINE__);
	}

	if ((errorFlags & TRACE_EVENT_DELAYED) != 0) {
		lib_printf("kernel (%s:%d): event delay detected at ts=%llu - event receiver couldn't keep up\n", __func__, __LINE__, eventDelayTimestamp);
	}

	if ((errorFlags & TRACE_BUFFER_WRITE_ERR) != 0) {
		lib_printf("kernel (%s:%d): buffer write error detected\n", __func__, __LINE__);
	}

	if (ret == EOK) {
		_trace_bufferFinish();
	}

	return ret;
}


int _perf_traceInit(vm_map_t *kmap)
{
	trace_common.running = 0;
	trace_common.stopped = 0;
	trace_common.gather = 0;
	trace_common.prev = 0;
	trace_common.epoch = 0;
	trace_common.errorFlags = 0;
	trace_common.eventDelayTimestamp = 0;

	hal_spinlockCreate(&trace_common.spinlock, "trace.spinlock");

	return trace_bufferInit(kmap);
}
