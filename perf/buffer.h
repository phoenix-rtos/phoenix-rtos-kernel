/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance analysis subsystem - event buffer
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#ifndef _TRACE_BUFFER_H_
#define _TRACE_BUFFER_H_


#include "vm/map.h"


extern int trace_bufferInit(vm_map_t *kmap);


extern int _trace_bufferStart(void);


extern int _trace_bufferFinish(void);


extern int _trace_bufferRead(u8 chan, void *buf, size_t bufsz);


extern int _trace_bufferWrite(u8 chan, const void *data, size_t sz);


extern int _trace_bufferWaitUntilAvail(u8 chan, size_t sz);


/* returns bytes available to write */
extern int _trace_bufferAvail(u8 chan);


extern int _trace_bufferDiscard(u8 chan, size_t sz);


#endif
