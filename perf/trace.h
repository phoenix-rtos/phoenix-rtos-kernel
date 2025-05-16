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


int _trace_init(vm_map_t *kmap);


int trace_start(unsigned flags);


int trace_read(u8 chan, void *buf, size_t bufsz);


int trace_stop(void);


int trace_finish(void);


int trace_isRunning(void);


#endif
