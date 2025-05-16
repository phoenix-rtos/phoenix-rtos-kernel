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
#include "trace-events.h"
#include "trace.h"


static struct {
	/*
	 * Treat `running` as atomic to reduce overhead on the kernel when the tracing is disabled
	 * - there is only one writer at a time (perf_trace{Start,Finish}()) and multiple readers
	 * (trace events doing trace_isRunning()). Due to eventual consistency in the readers
	 * we may lose some events, but we may lose them anyway as the invocation of
	 * trace_start() naturally races with kernel events occurring in the meantime.
	 *
	 * `running` under spinlock is always consistent.
	 */
	volatile int running;
	spinlock_t spinlock;

	/* guarded by spinlock */
	int stopped;
	int startPending; /* trace_start guard flag */

	int epoch;
	u32 prev;
	unsigned int flags;

	u8 errorFlags;
	u64 eventDelayCount;
	u64 eventDelayTimestamp;
	u64 startTimestamp;
} trace_common;


#define TRACE_NON_MONOTONICITY (1U << 1)
#define TRACE_EVENT_DELAYED    (1U << 2)
#define TRACE_BUFFER_WRITE_ERR (1U << 3)


static u32 _gettimeRaw(void)
{
	/* Intentional downcast to u32 - traces >1h are not supported */
	u32 now = (u32)hal_timerGetUs();

	while (now < trace_common.prev) {
		trace_common.errorFlags |= TRACE_NON_MONOTONICITY;
		now = (u32)hal_timerGetUs();
	}

	trace_common.prev = now;

	return now;
}


static void _writeEvent(u8 cpuChan, u8 event, const void *data, size_t sz, u32 *ts)
{
	u32 eventTs;
	ssize_t ret, try = 0;
	size_t eventSz = sizeof(eventTs) + sizeof(event) + sz;
	size_t avail;
	u8 chan = cpuChan + (u8)hal_cpuGetID() * (u8)trace_channel_count;

	struct {
		u32 ts;
		u8 eventId;
	} __attribute__((packed)) ev;

	if (ts == NULL || *ts == 0U) {
		eventTs = _gettimeRaw();
		if (ts != NULL) {
			*ts = eventTs;
		}
	}
	else {
		/* use timestamp provided by the caller */
		eventTs = *ts;
	}

	ret = _trace_bufferAvail(chan);
	if (ret < 0) {
		trace_common.errorFlags |= TRACE_BUFFER_WRITE_ERR;
		return;
	}

	avail = (size_t)ret;
	if (avail < eventSz) {
		if ((trace_common.flags & PERF_TRACE_FLAG_ROLLING) != 0U) {
			(void)_trace_bufferDiscard(chan, eventSz - avail);
		}
		else {
			try = _trace_bufferWaitUntilAvail(chan, eventSz);
		}
	}

	ev.ts = eventTs;
	ev.eventId = event;
	ret = _trace_bufferWrite(chan, &ev, sizeof(ev));
	if (ret == (ssize_t)sizeof(ev)) {
		ret = _trace_bufferWrite(chan, data, sz);
	}

	if (ret < 0) {
		trace_common.errorFlags |= TRACE_BUFFER_WRITE_ERR;
	}
	if (try > 0) {
		/*
		 * Record first occurrence of event delay to caution the user about possible
		 * loss of timestamp precision. This may happen if e.g. the buffer is implemented as RTT
		 * and the receiver (debug probe) can't keep up with the event generation rate
		 */
		trace_common.errorFlags |= TRACE_EVENT_DELAYED;
		trace_common.eventDelayCount++;
		trace_common.eventDelayTimestamp = _gettimeRaw();
	}
}


/* WARN: should be callable from interrupt handler */
void trace_writeEvent(u8 cpuChan, u8 event, const void *data, size_t sz, u32 *ts)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	if (trace_common.running != 0) {
		_writeEvent(cpuChan, event, data, sz, ts);
	}
	hal_spinlockClear(&trace_common.spinlock, &sc);
}


void _trace_updateLockEpoch(lock_t *lock)
{
	int prev = _proc_lockSetTraceEpoch(lock, trace_common.epoch);

	if (prev != trace_common.epoch) {
		_trace_eventLockName(lock);
	}
}


/* WARN: eventually consistent */
int trace_isRunning(void)
{
	return trace_common.running;
}


static void _emitThreadsCb(void *arg, int i, threadinfo_t *tinfo)
{
	struct {
		u16 pid;
		u16 tid;
		u8 priority;
		char name[128];
	} __attribute__((packed)) ev;

	ev.tid = (u16)tinfo->tid;
	ev.priority = (u8)tinfo->priority;
	ev.pid = (u16)tinfo->pid;

	hal_memcpy(ev.name, tinfo->name, sizeof(tinfo->name));

	_writeEvent((u8)trace_channel_meta, TRACE_EVENT_THREAD_CREATE, &ev, sizeof(ev), NULL);
}


static void _emitThreadinfo(void)
{
	(void)proc_threadsIter(0xffff, _emitThreadsCb, NULL);
}


static void _enableTracing(int enable)
{
	trace_common.running = enable;
	_hal_interruptsTrace(enable);
}


static int getChannelCount(void)
{
	return (int)hal_cpuGetCount() * (int)trace_channel_count;
}


int trace_start(unsigned int flags)
{
	spinlock_ctx_t sc;
	int ret;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	if (trace_common.running != 0 || trace_common.startPending != 0) {
		hal_spinlockClear(&trace_common.spinlock, &sc);
		return -EINPROGRESS;
	}
	trace_common.startPending = 1;
	hal_spinlockClear(&trace_common.spinlock, &sc);

	ret = _trace_bufferStart();
	if (ret < 0) {
		hal_spinlockSet(&trace_common.spinlock, &sc);
		trace_common.startPending = 0;
		hal_spinlockClear(&trace_common.spinlock, &sc);
		return ret;
	}

	if (_trace_bufferDiscard(0, 0) == -ENOSYS) {
		/* If discarding is unsupported by the buffer backend, ignore the flag */
		flags &= ~PERF_TRACE_FLAG_ROLLING;
	}

	/* Must be set before _emitThreadinfo as it depends on flags (via _writeEvent) */
	trace_common.flags = flags;

	/* Without spinlock - trace is not enabled yet, so there's no concurrent access */
	_emitThreadinfo();

	trace_common.errorFlags = 0;
	trace_common.eventDelayCount = 0;

	trace_common.epoch++;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	trace_common.startPending = 0;
	_enableTracing(1);
	trace_common.startTimestamp = _gettimeRaw();
	hal_spinlockClear(&trace_common.spinlock, &sc);

	return (int)getChannelCount();
}


int trace_read(u8 chan, void *buf, size_t bufsz)
{
	spinlock_ctx_t sc;
	int ret, running;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	running = trace_common.running;
	if (chan < (u8)getChannelCount() && (running != 0 || trace_common.stopped != 0)) {
		ret = _trace_bufferRead(chan, buf, bufsz);
	}
	else {
		ret = -EINVAL;
	}
	hal_spinlockClear(&trace_common.spinlock, &sc);

	return ret;
}


int trace_stop(void)
{
	int ret = EOK, running;
	spinlock_ctx_t sc;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	running = trace_common.running;
	if (trace_common.stopped == 0 && running != 0) {
		_enableTracing(0);
		trace_common.stopped = 1;
		ret = getChannelCount();
	}
	else {
		ret = -EINVAL;
	}
	hal_spinlockClear(&trace_common.spinlock, &sc);

	return ret;
}


int trace_finish(void)
{
	spinlock_ctx_t sc;
	int ret = EOK;
	u8 errorFlags = 0;
	u64 eventDelayCount = 0;
	u64 eventDelayTimestamp = 0;
	u64 startTimestamp = 0;
	u64 stopTimestamp = 0;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	if (trace_common.running != 0 || trace_common.stopped != 0) {
		_enableTracing(0);
		trace_common.stopped = 0;
		errorFlags = trace_common.errorFlags;
		eventDelayCount = trace_common.eventDelayCount;
		trace_common.eventDelayCount = 0;

		startTimestamp = trace_common.startTimestamp;
		stopTimestamp = _gettimeRaw();
		eventDelayTimestamp = trace_common.eventDelayTimestamp;
	}
	else {
		ret = -EINVAL;
	}
	hal_spinlockClear(&trace_common.spinlock, &sc);

	if (ret == EOK) {
		if ((errorFlags & TRACE_NON_MONOTONICITY) != 0U) {
			lib_printf("kernel (%s:%d): timer non-monotonicity detected during event gathering\n", __func__, __LINE__);
		}

		if ((errorFlags & TRACE_EVENT_DELAYED) != 0U) {
			lib_printf("kernel (%s:%d): event delay detected %llu times - event receiver couldn't keep up\n", __func__, __LINE__, eventDelayCount);
			lib_printf("kernel (%s:%d): start ts=%lld delay ts=%lld stop ts=%lld\n", __func__, __LINE__, startTimestamp, eventDelayTimestamp, stopTimestamp);
		}

		if ((errorFlags & TRACE_BUFFER_WRITE_ERR) != 0U) {
			lib_printf("kernel (%s:%d): buffer write error detected\n", __func__, __LINE__);
		}

		ret = _trace_bufferFinish();
	}

	return ret;
}


int _trace_init(vm_map_t *kmap)
{
	trace_common.running = 0;
	trace_common.stopped = 0;
	trace_common.startPending = 0;

	trace_common.epoch = 0;
	trace_common.prev = 0;

	hal_spinlockCreate(&trace_common.spinlock, "trace.spinlock");

	return trace_bufferInit(kmap);
}
