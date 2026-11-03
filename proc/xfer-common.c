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
	size_t ofs;
} xferPlan_t;


/* Where a payload of a message type can go in i.raw or o.raw, past the fields of the type; -1 if it can't. */
static int _rawOffset(int type, msg_side_t side)
{
	const msg_t *m = NULL;

	switch (type) {
		case mtOpen:
		case mtClose:
			return (side == msg_side_in) ? (int)sizeof(m->i.openclose) : 0;

		case mtRead:
		case mtWrite:
		case mtTruncate:
			return (side == msg_side_in) ? (int)sizeof(m->i.io) : 0;

		case mtCreate:
			return (side == msg_side_in) ? (int)sizeof(m->i.create) : (int)sizeof(m->o.create);

		case mtSetAttr:
		case mtGetAttr:
			return (side == msg_side_in) ? (int)sizeof(m->i.attr) : (int)sizeof(m->o.attr);

		case mtLink:
		case mtUnlink:
			return (side == msg_side_in) ? (int)sizeof(m->i.ln) : 0;

		case mtReaddir:
			return (side == msg_side_in) ? (int)sizeof(m->i.readdir) : 0;

		case mtLookup:
			return (side == msg_side_in) ? 0 : (int)sizeof(m->o.lookup);

		case mtDestroy:
		case mtGetAttrAll:
			return 0;

		/* the raw areas of mtDevCtl and of the types the kernel doesn't know are the server's protocol */
		default:
			return -1;
	}
}


static xferPlan_t _classify(int type, msg_side_t side, size_t size)
{
	xferPlan_t plan = { msg_xfer_none, 0 };
	int rawofs;

	if (size == 0U) {
		return plan;
	}

	rawofs = _rawOffset(type, side);

	if ((rawofs >= 0) && (size <= MSG_RAW_SIZE - (size_t)rawofs)) {
		plan.kind = msg_xfer_raw;
		plan.ofs = (size_t)rawofs;
	}
	else {
		plan.kind = msg_xfer_map;
	}

	return plan;
}


static int _setupSide(thread_t *caller, thread_t *recv, const xferPlan_t *plan, void *data, size_t size, msg_side_t side, void **rdata)
{
	xferBuf_t *xb = (side == msg_side_in) ? &caller->ipc.call.ibl : &caller->ipc.call.obl;

	*rdata = NULL;

	switch (plan->kind) {
		case msg_xfer_raw:
			/* recv's msg is in its address space, only the address is taken here */
			*rdata = (side == msg_side_in) ? &recv->ipc.msgPtr->i.raw[plan->ofs] : &recv->ipc.msgPtr->o.raw[plan->ofs];
			return EOK;

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


int xfer_begin(thread_t *caller, thread_t *recv, int type, unsigned char *iraw, unsigned char *oraw, const void *idata, size_t isize, void *odata, size_t osize, void **ridata, void **rodata)
{
	xferPlan_t in = _classify(type, msg_side_in, isize);
	xferPlan_t out = _classify(type, msg_side_out, osize);
	int err = EOK;

	caller->ipc.call.ikind = (u8)in.kind;
	caller->ipc.call.okind = (u8)out.kind;
	caller->ipc.call.irawofs = (u8)in.ofs;
	caller->ipc.call.orawofs = (u8)out.ofs;
	caller->ipc.call.osize = osize;
	caller->ipc.call.odata = odata;

	err = _setupSide(caller, recv, &in, (void *)idata, isize, msg_side_in, ridata);
	if (err == EOK) {
		err = _setupSide(caller, recv, &out, odata, osize, msg_side_out, rodata);
	}
	if (err < 0) {
		_release(caller);
		return err;
	}

	if (in.kind == msg_xfer_raw) {
		hal_memcpy(iraw + in.ofs, idata, isize);
	}

	/* taken here, in the caller's aspace, for _msgDeliver() to put in recv's o.raw */
	if (out.kind == msg_xfer_raw) {
		hal_memcpy(oraw, odata, osize);
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
		case msg_xfer_raw:
			/* comes back in the reply's raw area, _xfer_copyOut() takes it from there */
			break;

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


void _xfer_copyOut(thread_t *caller)
{
	/* reply.size is what xfer_end() returned, at most the caller's o.size */
	if ((caller->ipc.call.okind == msg_xfer_raw) && (caller->ipc.reply.size != 0U)) {
		hal_memcpy(caller->ipc.call.odata, caller->ipc.reply.raw + caller->ipc.call.orawofs, caller->ipc.reply.size);
	}
}


void xfer_abort(thread_t *caller)
{
	if (__atomic_exchange_n(&caller->ipc.call.active, 0U, __ATOMIC_ACQ_REL) != 0U) {
		_release(caller);
	}
}
