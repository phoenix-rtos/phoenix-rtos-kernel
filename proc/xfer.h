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


#define MSG_IN_EXTRA  (1 << 0)
#define MSG_OUT_EXTRA (1 << 1)
#define MSG_IN_MAP    (1 << 2)
#define MSG_OUT_MAP   (1 << 3)


typedef enum {
	msg_xfer_none = 0, /* nothing to transfer */
	msg_xfer_extra,    /* payload fits into the receiver's IPC buffer */
	msg_xfer_borrow,   /* payload lives inside the caller's IPC buffer, which the receiver can temporarily borrow via _borrowBuf() */
	msg_xfer_map,      /* fallback - must create dedicated shared mapping via proc_setupSharedBuffer() */
} msg_xfer_t;

typedef enum {
	msg_side_in = 0,
	msg_side_out,
} msg_side_t;

typedef struct {
	msg_xfer_t kind;
	const void *data; /* original caller-supplied pointer, valid for msg_xfer_map */
	size_t size;
	size_t ofs; /* msg_xfer_borrow: offset in caller->utcb.w; msg_xfer_extra: offset in recv->utcb.w */
} xferPlan_t;


/* FIXME: not satisfied with the functions' naming. Will need to think it through */


void xfer_bufRelease(xferBuf_t *xb);


int xfer_bufMap(process_t *t, process_t *recv, void *data, size_t size, xferBuf_t *xb, void **rdata);


int xfer_ipcBufBorrow(thread_t *from, thread_t *to);


void xfer_ipcBufRelease(thread_t *t);


void xfer_init(vm_map_t *kmap);


xferPlan_t xfer_classify(thread_t *caller, thread_t *recv, const void *data, size_t size, size_t extraUsed);


int xfer_setup(thread_t *caller, thread_t *recv, const xferPlan_t *plan, xferBuf_t *xb, void **rdata, msg_side_t side);


void xfer_copyShadowPages(thread_t *from, thread_t *to, msg_t *msg);


static inline void xfer_clearFlags(thread_t *t)
{
	t->ipc.flags = 0;
}


static inline void xfer_copyOutExtra(thread_t *from, thread_t *to, msg_t *msg)
{
	if ((to->ipc.flags & MSG_OUT_EXTRA) != 0) {
		/* if i.data is also extra, the o.data is put next to it */
		size_t ofs = (to->ipc.flags & MSG_IN_EXTRA) != 0 ? msg->i.size : 0;

		hal_memcpy(to->ipc.msgPtr->o.data, from->ipc.kw + ofs, msg->o.size);
		xfer_clearFlags(to);
	}
}


#endif
