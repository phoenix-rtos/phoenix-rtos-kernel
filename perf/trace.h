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


extern int _trace_init(vm_map_t *kmap);


extern int trace_start(unsigned flags);


extern int trace_read(u8 chan, void *buf, size_t bufsz);


extern int trace_stop(void);


extern int trace_finish(void);


extern int trace_isRunning(void);


#endif
