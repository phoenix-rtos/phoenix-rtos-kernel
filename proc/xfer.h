/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Message transfer
 *
 * Copyright 2026 Phoenix Systems
 * Author: Adam Greloch
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#ifndef _PH_PROC_XFER_H_
#define _PH_PROC_XFER_H_

#include "hal/hal.h"
#include "vm/vm.h"
#include "process.h"
#include "threads.h"


typedef enum {
	msg_xfer_none = 0, /* nothing to transfer */
	msg_xfer_map,      /* payload pages are mapped into the receiver (fallback) */
} msg_xfer_t;

typedef enum {
	msg_side_in = 0,
	msg_side_out,
} msg_side_t;


int xfer_begin(thread_t *caller, thread_t *recv, const void *idata, size_t isize, void *odata, size_t osize, void **ridata, void **rodata);


size_t xfer_end(thread_t *caller, size_t osize);


/* Releases the call without writing anything back. */
void xfer_abort(thread_t *caller);


void xfer_releaseIpcBuf(thread_t *thread);


void xfer_init(vm_map_t *kmap);


/* Implemented per memory model, used by xfer-common.c only. */


int xfer_bufMap(process_t *src, process_t *dst, void *data, size_t size, msg_side_t side, xferBuf_t *xb, void **rdata);


void xfer_bufCopyBack(xferBuf_t *xb, size_t size);


void xfer_bufRelease(xferBuf_t *xb);


#endif
