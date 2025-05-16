/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance analysis subsystem - event buffer implementation using RTT
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */


#include "include/errno.h"
#include "include/perf.h"
#include "hal/arm/rtt.h"

#include "buffer.h"

#include <board_config.h>

#ifndef RTT_TRACE_META_CHANNEL
#define RTT_TRACE_META_CHANNEL 2
#endif

#ifndef RTT_TRACE_EVENT_CHANNEL
#define RTT_TRACE_EVENT_CHANNEL 3
#endif

#ifndef PERF_RTT_ENABLED
#define PERF_RTT_ENABLED 0
#endif

#if PERF_RTT_ENABLED && (!defined(RTT_ENABLED_PLO) || !RTT_ENABLED_PLO || !defined(RTT_ENABLED) || !RTT_ENABLED)
#error "RTT_ENABLED requires RTT_ENABLED_PLO"
#endif

#ifndef RTT_PERF_BUFFERS
#define RTT_PERF_BUFFERS 0
#endif

#if PERF_RTT_ENABLED && !RTT_PERF_BUFFERS
#error "buffer-rtt used but RTT_PERF_BUFFERS is disabled"
#endif


static struct {
	int initialized;
	struct {
		u8 rtt;
	} chans[trace_channel_count];
} buffer_common;


int _trace_bufferStart(void)
{
	if (buffer_common.initialized == 0) {
		return -ENOSYS;
	}

	return EOK;
}


int _trace_bufferRead(u8 chan, void *buf, size_t bufsz)
{
	return 0;
}


int _trace_bufferWrite(u8 chan, const void *data, size_t sz)
{
	if (buffer_common.initialized == 0) {
		return -EINVAL;
	}

	return _hal_rttWrite(buffer_common.chans[chan].rtt, data, sz);
}


int _trace_bufferWaitUntilAvail(u8 chan, size_t sz)
{
	int try = 0;

	while (_hal_rttTxAvail(buffer_common.chans[chan].rtt) < sz) {
		try++;
	};

	return try;
}


int _trace_bufferAvail(u8 chan)
{
	return _hal_rttTxAvail(buffer_common.chans[chan].rtt);
}


int _trace_bufferDiscard(u8 chan, size_t sz)
{
	return -ENOSYS;
}


int _trace_bufferFinish(void)
{
	if (buffer_common.initialized == 0) {
		return -ENOSYS;
	}

	return EOK;
}


int trace_bufferInit(vm_map_t *kmap)
{
	buffer_common.initialized = 0;

#if PERF_RTT_ENABLED
	if (_hal_rttSetup() < 0) {
		return -1;
	}

	buffer_common.initialized = 1;

	buffer_common.chans[trace_channel_event].rtt = RTT_TRACE_EVENT_CHANNEL;
	buffer_common.chans[trace_channel_meta].rtt = RTT_TRACE_META_CHANNEL;
#else
	return -ENOSYS;
#endif

	return EOK;
}
