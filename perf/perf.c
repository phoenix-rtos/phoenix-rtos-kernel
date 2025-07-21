/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance analysis subsystem
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#include "include/errno.h"
#include "proc/threads.h"

#include "trace.h"
#include "perf.h"


int _perf_init(vm_map_t *kmap)
{
	return _perf_traceInit(kmap);
}


int perf_start(perf_mode_t mode, unsigned flags, void *arg)
{
	switch (mode) {
		case perf_mode_threads:
			return perf_threadsStart((unsigned)arg);
		case perf_mode_trace:
			return perf_traceStart(flags);
		default:
			return -ENOSYS;
	}
}


int perf_read(perf_mode_t mode, void *buf, size_t bufsz, int chan)
{
	switch (mode) {
		case perf_mode_threads:
			return perf_threadsRead(buf, bufsz);
		case perf_mode_trace:
			return perf_traceRead(chan, buf, bufsz);
		default:
			return -ENOSYS;
	}
}


int perf_stop(perf_mode_t mode)
{
	switch (mode) {
		case perf_mode_trace:
			return perf_traceStop();
		default:
			return -ENOSYS;
	}
}


int perf_finish(perf_mode_t mode)
{
	switch (mode) {
		case perf_mode_threads:
			return perf_threadsFinish();
		case perf_mode_trace:
			return perf_traceFinish();
		default:
			return -ENOSYS;
	}
}
