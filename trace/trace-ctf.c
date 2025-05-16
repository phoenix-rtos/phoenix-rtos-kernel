/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance tracing subsystem - CTF backend
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

	int gather;

	u32 filter;

	int nonMonotonicity;
} trace_common;


u64 _trace_gettimeRaw(void)
{
	u64 now = hal_timerGetUs();


	if (now < trace_common.prev) {
		/* defer lib assertion prints on non-monotonicity as we may be inside the interrupt handler */
		trace_common.nonMonotonicity = 1;
	}

	trace_common.prev = now;

	return now;
}


void trace_eventsU8(u8 event, u8 val)
{
	struct {
		u64 ts;
		u32 event;
		u32 val;
	} __attribute__((packed)) ev;

	if (trace_common.gather == 0) {
		return;
	}

	ev.ts = _trace_gettimeRaw();
	ev.event = event;
	ev.val = val;

	trace_bufferWrite(&ev, sizeof(ev));
}


void trace_eventsU32Str(u8 event, u32 val, const char *str)
{
	struct {
		u64 ts;
		u32 event;
		u32 val;
		char buf[16];
	} __attribute__((packed)) ev;

	if (trace_common.gather == 0) {
		return;
	}

	ev.ts = _trace_gettimeRaw();
	ev.event = event;
	ev.val = val;
	hal_strcpy(ev.buf, str);

	trace_bufferWrite(&ev, sizeof(ev));
}


int trace_start(void)
{
	int ret;

	ret = trace_bufferStart();
	if (ret < 0) {
		return ret;
	}

	trace_common.gather = 1;
	trace_common.nonMonotonicity = 0;

	hal_interruptsTrace(1);

	return EOK;
}


int trace_read(void *buf, size_t bufsz)
{
	return trace_bufferRead(buf, bufsz);
}


int trace_finish(void)
{
	LIB_ASSERT(trace_common.nonMonotonicity == 0, "timer non-monotonicity detected during event gathering");

	trace_common.gather = 0;

	trace_bufferFinish();

	hal_interruptsTrace(0);

	return EOK;
}


int _trace_init(vm_map_t *kmap)
{
	trace_common.gather = 0;
	trace_common.prev = 0;

	return trace_bufferInit(kmap);
}
