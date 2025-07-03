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
#include "hal/arm/rtt.h"

#include "buffer.h"

#include <board_config.h>

#ifndef RTT_PERF_CHANNEL
#define RTT_PERF_CHANNEL 1
#endif

#if RTT_ENABLED && !RTT_ENABLED_PLO
#error "RTT_ENABLED requires RTT_ENABLED_PLO"
#endif


static struct {
	int initialized;
} common;


int _trace_bufferStart(void)
{
	if (common.initialized == 0) {
		return -ENOSYS;
	}

	return EOK;
}


int _trace_bufferRead(void *buf, size_t bufsz)
{
	return 0;
}


int _trace_bufferWrite(const void *data, size_t sz)
{
	if (common.initialized == 0) {
		return -EINVAL;
	}

	return _hal_rttWrite(RTT_PERF_CHANNEL, data, sz);
}


int _trace_bufferWaitUntilAvail(size_t sz)
{
	int try = 0;

	while (_hal_rttTxAvail(RTT_PERF_CHANNEL) < sz) {
		try++;
	};

	return try;
}


int _trace_bufferFinish(void)
{
	if (common.initialized == 0) {
		return -ENOSYS;
	}

	return EOK;
}


int trace_bufferInit(vm_map_t *kmap)
{
	common.initialized = 0;

	if (_hal_rttInitialized() == 0) {
		/* RTT may be still uninitialized, e.g. if the RTT console is disabled */
		if (_hal_rttInit() < 0) {
			return -1;
		}
	}

	common.initialized = 1;

	return EOK;
}
