/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Message transfer (common core)
 *
 * Copyright 2026 Phoenix Systems
 * Author: Adam Greloch
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "xfer.h"

#include "include/errno.h"


typedef struct {
	msg_xfer_t kind;
} xferPlan_t;


static xferPlan_t _classify(size_t size)
{
	xferPlan_t plan = { msg_xfer_none };

	if (size == 0U) {
		return plan;
	}

	plan.kind = msg_xfer_map;

	return plan;
}


static int _setupSide(thread_t *caller, thread_t *recv, const xferPlan_t *plan, void *data, size_t size, msg_side_t side, void **rdata)
{
	xferBuf_t *xb = (side == msg_side_in) ? &caller->ipc.call.ibl : &caller->ipc.call.obl;

	*rdata = NULL;

	switch (plan->kind) {
		case msg_xfer_map:
			return xfer_bufMap(caller->process, recv->process, data, size, side, xb, rdata);

		default:
			return EOK;
	}
}


static void _release(thread_t *caller)
{
	xfer_bufRelease(&caller->ipc.call.ibl);
	xfer_bufRelease(&caller->ipc.call.obl);
}


int xfer_begin(thread_t *caller, thread_t *recv, const void *idata, size_t isize, void *odata, size_t osize, void **ridata, void **rodata)
{
	xferPlan_t in = _classify(isize);
	xferPlan_t out = _classify(osize);
	int err = EOK;

	caller->ipc.call.ikind = (u8)in.kind;
	caller->ipc.call.okind = (u8)out.kind;
	caller->ipc.call.osize = osize;

	err = _setupSide(caller, recv, &in, (void *)idata, isize, msg_side_in, ridata);
	if (err == EOK) {
		err = _setupSide(caller, recv, &out, odata, osize, msg_side_out, rodata);
	}
	if (err < 0) {
		_release(caller);
		return err;
	}

	__atomic_store_n(&caller->ipc.call.active, 1U, __ATOMIC_RELEASE);

	return EOK;
}


size_t xfer_end(thread_t *caller, size_t osize)
{
	size_t size = min(osize, caller->ipc.call.osize);

	/* lost to xfer_abort() of a dying caller */
	if (__atomic_exchange_n(&caller->ipc.call.active, 0U, __ATOMIC_ACQ_REL) == 0U) {
		return 0;
	}

	switch (caller->ipc.call.okind) {
		case msg_xfer_map:
			/* the receiver could have written anywhere in the mapping */
			xfer_bufCopyBack(&caller->ipc.call.obl, caller->ipc.call.osize);
			break;

		default:
			break;
	}

	_release(caller);

	return size;
}


void xfer_abort(thread_t *caller)
{
	if (__atomic_exchange_n(&caller->ipc.call.active, 0U, __ATOMIC_ACQ_REL) != 0U) {
		_release(caller);
	}
}
