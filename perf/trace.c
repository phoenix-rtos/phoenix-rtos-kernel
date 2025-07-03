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
	u64 prev;
	int epoch;
	u8 errorFlags;
	u64 eventDelayTimestamp;
} trace_common;


#define TRACE_NON_MONOTONICITY (1 << 1)
#define TRACE_EVENT_DELAYED    (1 << 2)
#define TRACE_BUFFER_WRITE_ERR (1 << 3)


static u64 _perf_traceGettimeRaw(void)
{
	u64 now = hal_timerGetUs();

	while (now < trace_common.prev) {
		trace_common.errorFlags |= TRACE_NON_MONOTONICITY;
		now = hal_timerGetUs();
	}

	trace_common.prev = now;

	return now;
}


/* WARN: should be callable from interrupt handler */
void perf_traceEventsWrite(u8 event, const void *data, size_t sz, u64 *ts)
{
	spinlock_ctx_t sc;
	u64 eventTs;
	unsigned int wbytes = 0;
	int ret, retries;
	int eventSz = sizeof(eventTs) + sizeof(event) + sz;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	if (trace_common.running != 0) {
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
			retries = _trace_bufferWaitUntilAvail(eventSz);

			ret = _trace_bufferWrite(&eventTs, sizeof(eventTs));
			if (ret != sizeof(eventTs)) {
				break;
			}
			wbytes += ret;

			ret = _trace_bufferWrite(&event, sizeof(event));
			if (ret != sizeof(event)) {
				break;
			}
			wbytes += ret;

			ret = _trace_bufferWrite(data, sz);
			if (ret != sz) {
				break;
			}
			wbytes += ret;
		} while (0);

		if (ret < 0) {
			trace_common.errorFlags |= TRACE_BUFFER_WRITE_ERR;
		}
		else if ((trace_common.errorFlags & TRACE_EVENT_DELAYED) == 0 && retries > 0) {
			/*
			 * Record first occurrence of event delay to caution the user about possible
			 * loss of timestamp precision. This may happen if e.g. the buffer is implemented as RTT
			 * and the receiver (debug probe) can't keep up with the event generation rate
			 */
			trace_common.errorFlags |= TRACE_EVENT_DELAYED;
			trace_common.eventDelayTimestamp = eventTs;
		}
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
	_trace_bufferWrite(tinfo, sizeof(threadinfo_t));
}


static void _perf_emitThreadinfo(void)
{
	u32 n, tcnt = proc_threadCount();

	_trace_bufferWrite(&tcnt, sizeof(tcnt));
	n = proc_threadsIter(0xFFFF, _perf_traceEmitThreadsCb, NULL);

	LIB_ASSERT(n == tcnt, "thread count mismatch: %d != %d", n, tcnt);
}


int perf_traceStart(void)
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

		_perf_emitThreadinfo();

		trace_common.running = 1;
		trace_common.gather = 1;
		trace_common.errorFlags = 0;
		trace_common.epoch++;

		_hal_interruptsTrace(1);
	} while (0);
	hal_spinlockClear(&trace_common.spinlock, &sc);

	return ret;
}


int perf_traceRead(void *buf, size_t bufsz)
{
	spinlock_ctx_t sc;
	int ret;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	if (trace_common.running != 0) {
		ret = _trace_bufferRead(buf, bufsz);
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
	u64 eventDelayTimestamp;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	if (trace_common.running != 0) {
		trace_common.running = 0;
		trace_common.gather = 0;
		errorFlags = trace_common.errorFlags;
		eventDelayTimestamp = trace_common.eventDelayTimestamp;
		_hal_interruptsTrace(0);
		_trace_bufferFinish();
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

	return ret;
}


int _perf_traceInit(vm_map_t *kmap)
{
	trace_common.running = 0;
	trace_common.gather = 0;
	trace_common.prev = 0;
	trace_common.epoch = 0;
	trace_common.errorFlags = 0;
	trace_common.eventDelayTimestamp = 0;

	hal_spinlockCreate(&trace_common.spinlock, "trace.spinlock");

	return trace_bufferInit(kmap);
}
