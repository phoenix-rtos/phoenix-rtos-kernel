/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance tracing subsystem - event buffer
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#ifndef _TRACE_BUFFER_H_
#define _TRACE_BUFFER_H_


#include "vm/vm.h"


extern int trace_bufferInit(vm_map_t *kmap);


extern int trace_bufferStart(void);


extern int trace_bufferFinish(void);


extern int trace_bufferRead(void *buf, size_t bufsz);


extern void trace_bufferWrite(void *data, size_t sz);


#endif
