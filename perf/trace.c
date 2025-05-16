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


struct {
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
	time_t prev;
	int epoch;
	int nonMonotonicity;
	int bufferFull;
	time_t bufferFullTimestamp;
} trace_common;


static u64 _perf_traceGettimeRaw(void)
{
	u64 now = hal_timerGetUs();

	while (now < trace_common.prev) {
		trace_common.nonMonotonicity = 1;
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

		wbytes += _trace_bufferWrite(&eventTs, sizeof(eventTs));
		wbytes += _trace_bufferWrite(&event, sizeof(event));
		wbytes += _trace_bufferWrite(data, sz);

		if (trace_common.bufferFull == 0 && wbytes < sizeof(ts) + sizeof(event) + sz) {
			/* record first occurrence of buffer overflow to caution the user about possible loss of events */
			trace_common.bufferFull = 1;
			trace_common.bufferFullTimestamp = eventTs;
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

		trace_common.running = 1;
		trace_common.gather = 1;
		trace_common.nonMonotonicity = 0;
		trace_common.bufferFull = 0;
		trace_common.epoch++;

		_hal_interruptsTrace(1);
	} while (0);
	hal_spinlockClear(&trace_common.spinlock, &sc);

	return EOK;
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
	int ret = EOK, nonMonotonicity = 0, bufferFull = 0;
	time_t bufferFullTimestamp;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	if (trace_common.running != 0) {
		trace_common.gather = 0;
		nonMonotonicity = trace_common.nonMonotonicity;
		bufferFull = trace_common.bufferFull;
		bufferFullTimestamp = trace_common.bufferFullTimestamp;
		_trace_bufferFinish();
		_hal_interruptsTrace(0);
	}
	else {
		ret = -EINVAL;
	}
	hal_spinlockClear(&trace_common.spinlock, &sc);

	if (nonMonotonicity != 0) {
		lib_printf("kernel (%s:%d): timer non-monotonicity detected during event gathering", __func__, __LINE__);
	}

	if (bufferFull != 0) {
		lib_printf("kernel (%s:%d): first event buffer overflow detected at ts=%d - some events were corrupted/lost", __func__, __LINE__, bufferFullTimestamp);
	}

	return ret;
}


int _perf_traceInit(vm_map_t *kmap)
{
	trace_common.running = 0;
	trace_common.gather = 0;
	trace_common.prev = 0;
	trace_common.epoch = 0;

	hal_spinlockCreate(&trace_common.spinlock, "trace.spinlock");

	return trace_bufferInit(kmap);
}
