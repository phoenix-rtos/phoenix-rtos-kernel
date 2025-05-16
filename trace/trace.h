/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance tracing subsystem
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#ifndef _TRACE_TRACE_H_
#define _TRACE_TRACE_H_


#include "vm/vm.h"


extern int _trace_init(vm_map_t *kmap);


extern int trace_start(void);


extern int trace_read(void *buf, size_t bufsz);


extern int trace_finish(void);


#endif
