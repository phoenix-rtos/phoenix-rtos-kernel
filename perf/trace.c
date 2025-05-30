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
	time_t lastTimestamp;
	time_t prev;

	spinlock_t spinlock;

	int gather;

	u32 filter;

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
void perf_traceEventsWrite(u8 event, void *data, size_t sz)
{
	spinlock_ctx_t sc;
	u64 ts;
	unsigned int wbytes = 0;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	ts = _perf_traceGettimeRaw();

	wbytes += _trace_bufferWrite(&ts, sizeof(ts));
	wbytes += _trace_bufferWrite(&event, sizeof(event));
	wbytes += _trace_bufferWrite(data, sz);

	if (trace_common.bufferFull == 0 && wbytes < sizeof(ts) + sizeof(event) + sz) {
		/* record first occurrence of buffer overflow to caution the user about possible loss of events */
		trace_common.bufferFull = 1;
		trace_common.bufferFullTimestamp = ts;
	}

	hal_spinlockClear(&trace_common.spinlock, &sc);
}


int perf_traceIsRunning(void)
{
	return trace_common.gather;
}


static void perf_traceDumpLocks(void)
{
	proc_locksIter(perf_traceEventsLockName);
}


int perf_traceStart(void)
{
	spinlock_ctx_t sc;
	int ret;

	if (trace_common.gather == 1) {
		return -EINVAL;
	}

	hal_spinlockSet(&trace_common.spinlock, &sc);
	ret = _trace_bufferStart();
	if (ret < 0) {
		return ret;
	}

	trace_common.gather = 1;
	trace_common.nonMonotonicity = 0;
	trace_common.bufferFull = 0;

	_hal_interruptsTrace(1);
	hal_spinlockClear(&trace_common.spinlock, &sc);

	/* emit names of already set/cleared locks */
	perf_traceDumpLocks();

	return EOK;
}


int perf_traceRead(void *buf, size_t bufsz)
{
	spinlock_ctx_t sc;
	int ret;

	if (trace_common.gather == 0) {
		return -EINVAL;
	}

	hal_spinlockSet(&trace_common.spinlock, &sc);
	ret = _trace_bufferRead(buf, bufsz);
	hal_spinlockClear(&trace_common.spinlock, &sc);

	return ret;
}


int perf_traceFinish(void)
{
	spinlock_ctx_t sc;

	if (trace_common.gather == 0) {
		return -EINVAL;
	}

	if (trace_common.nonMonotonicity != 0) {
		lib_printf("kernel (%s:%d): timer non-monotonicity detected during event gathering", __func__, __LINE__);
	}

	if (trace_common.bufferFull != 0) {
		lib_printf("kernel (%s:%d): first event buffer overflow detected at ts=%d - some events were corrupted/lost", __func__, __LINE__, trace_common.bufferFullTimestamp);
	}

	trace_common.gather = 0;

	hal_spinlockSet(&trace_common.spinlock, &sc);
	_trace_bufferFinish();
	_hal_interruptsTrace(0);
	hal_spinlockClear(&trace_common.spinlock, &sc);

	return EOK;
}


int _perf_traceInit(vm_map_t *kmap)
{
	trace_common.gather = 0;
	trace_common.prev = 0;

	hal_spinlockCreate(&trace_common.spinlock, "trace.spinlock");

	return trace_bufferInit(kmap);
}
