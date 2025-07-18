/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance analysis subsystem - event tracing
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#ifndef _PERF_TRACE_H_
#define _PERF_TRACE_H_

#include "vm/map.h"


extern int _perf_traceInit(vm_map_t *kmap);


extern int perf_traceStart(void);


extern int perf_traceRead(void *buf, size_t bufsz);


extern int perf_traceStop(void);


extern int perf_traceFinish(void);


extern int perf_traceIsRunning(void);


#endif
