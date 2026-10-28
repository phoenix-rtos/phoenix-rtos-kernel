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
	size_t ofs; /* msg_xfer_borrow: offset in caller->ipc.w; msg_xfer_extra: offset in recv->ipc.w */
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


static xferPlan_t _classify(const thread_t *caller, const thread_t *recv, int type, msg_side_t side, const void *data, size_t size, size_t extraUsed)
{
	xferPlan_t plan = { msg_xfer_none, 0 };
	const char *w = caller->ipc.w;
	int rawofs;

	if (size == 0U) {
		return plan;
	}

	rawofs = _rawOffset(type, side);

	/* travels with the message itself, nothing to map or copy */
	if ((rawofs >= 0) && (size <= MSG_RAW_SIZE - (size_t)rawofs)) {
		plan.kind = msg_xfer_raw;
		plan.ofs = (size_t)rawofs;
	}
	/* a held buffer still has a previous caller's output in it */
	else if ((size + extraUsed <= recv->ipc.size) && (__atomic_load_n(&recv->ipc.heldBy, __ATOMIC_ACQUIRE) == NULL)) {
		plan.kind = msg_xfer_extra;
		plan.ofs = extraUsed;
	}
	else if ((w != NULL) && (size <= caller->ipc.size) && ((const char *)data >= w) &&
			((const char *)data + size <= w + caller->ipc.size)) {
		plan.kind = msg_xfer_borrow;
		plan.ofs = (size_t)((const char *)data - w);
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

		case msg_xfer_extra:
			*rdata = (char *)recv->ipc.w + plan->ofs;
			return EOK;

		case msg_xfer_borrow:
			*rdata = (char *)caller->ipc.call.bw + plan->ofs;
			return EOK;

		case msg_xfer_map:
			return xfer_bufMap(caller->process, recv->process, data, size, side, xb, rdata);

		default:
			return EOK;
	}
}


/* Where the output is in the receiver's or the borrowed IPC buffer. */
static size_t _oofs(const thread_t *caller)
{
	if (caller->ipc.call.okind == msg_xfer_borrow) {
		return (size_t)((const char *)caller->ipc.call.odata - (const char *)caller->ipc.w);
	}

	/* msg_xfer_extra, after the input if that is extra too */
	return (caller->ipc.call.ikind == msg_xfer_extra) ? caller->ipc.call.isize : 0U;
}


static void _release(thread_t *caller)
{
	xfer_bufRelease(&caller->ipc.call.ibl);
	xfer_bufRelease(&caller->ipc.call.obl);
	xfer_borrowRelease(caller);

	/* ikind and okind stay, a pending output's offset is derived from them */
}


int xfer_begin(thread_t *caller, thread_t *recv, int type, unsigned char *iraw, const void *idata, size_t isize, void *odata, size_t osize, void **ridata, void **rodata)
{
	xferPlan_t in = _classify(caller, recv, type, msg_side_in, idata, isize, 0);
	xferPlan_t out = _classify(caller, recv, type, msg_side_out, odata, osize, (in.kind == msg_xfer_extra) ? isize : 0U);
	int err = EOK;

	caller->ipc.call.ikind = (u8)in.kind;
	caller->ipc.call.okind = (u8)out.kind;
	caller->ipc.call.irawofs = (u8)in.ofs;
	caller->ipc.call.orawofs = (u8)out.ofs;
	caller->ipc.call.isize = isize;
	caller->ipc.call.osize = osize;
	caller->ipc.call.odata = odata;

	if ((in.kind == msg_xfer_borrow) || (out.kind == msg_xfer_borrow)) {
		err = xfer_borrowMap(caller, recv);
	}
	if (err == EOK) {
		err = _setupSide(caller, recv, &in, (void *)idata, isize, msg_side_in, ridata);
	}
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
	else if (in.kind == msg_xfer_extra) {
		hal_memcpy(recv->ipc.kw, idata, isize);
	}

	__atomic_store_n(&caller->ipc.call.active, 1U, __ATOMIC_RELEASE);

	return EOK;
}


int xfer_forward(thread_t *caller, thread_t *recv, thread_t *forward, msg_t *msg)
{
	size_t used = 0;

	/* the payload is handed over with msg, it only has to point into forward's copy */
	if (caller->ipc.call.ikind == msg_xfer_raw) {
		msg->i.data = &forward->ipc.msgPtr->i.raw[caller->ipc.call.irawofs];
	}
	if (caller->ipc.call.okind == msg_xfer_raw) {
		msg->o.data = &forward->ipc.msgPtr->o.raw[caller->ipc.call.orawofs];
	}

	if (caller->ipc.call.okind == msg_xfer_extra) {
		used = _oofs(caller) + caller->ipc.call.osize;
	}
	else if (caller->ipc.call.ikind == msg_xfer_extra) {
		used = caller->ipc.call.isize;
	}
	else {
		return EOK;
	}

	/* recv reuses its IPC buffer as soon as it is done forwarding */
	if (used > forward->ipc.size) {
		return -ENOMEM;
	}

	xfer_flush(forward);

	hal_memcpy(forward->ipc.kw, recv->ipc.kw, used);

	if (caller->ipc.call.ikind == msg_xfer_extra) {
		msg->i.data = forward->ipc.w;
		msg->i.size = caller->ipc.call.isize;
	}
	if (caller->ipc.call.okind == msg_xfer_extra) {
		msg->o.data = (char *)forward->ipc.w + _oofs(caller);
		msg->o.size = caller->ipc.call.osize;
	}

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
		case msg_xfer_extra:
			if (size > 0U) {
				/* the responder is the one holding the output, forward moved it there */
				LIB_ASSERT(_oofs(caller) + size <= proc_current()->ipc.size, "output not in the responder's IPC buffer");
				caller->ipc.call.osize = size;
				caller->ipc.call.owner = proc_current();
				__atomic_store_n(&proc_current()->ipc.heldBy, caller, __ATOMIC_RELEASE);
			}
			break;

		case msg_xfer_raw:
			/* comes back in the reply's raw area, _xfer_copyOut() takes it from there */
			break;

		case msg_xfer_borrow:
			xfer_borrowCopyBack(caller, _oofs(caller), size);
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


static void _unhold(thread_t *caller)
{
	__atomic_store_n(&caller->ipc.call.owner->ipc.heldBy, NULL, __ATOMIC_RELEASE);
	caller->ipc.call.owner = NULL;
}


static const void *_pendingSrc(const thread_t *caller)
{
	return (const char *)caller->ipc.call.owner->ipc.kw + _oofs(caller);
}


void _xfer_copyOut(thread_t *caller)
{
	/* reply.size is what xfer_end() returned, at most the caller's o.size */
	if ((caller->ipc.call.okind == msg_xfer_raw) && (caller->ipc.reply.size != 0U)) {
		hal_memcpy(caller->ipc.call.odata, caller->ipc.reply.raw + caller->ipc.call.orawofs, caller->ipc.reply.size);
	}
	else if (caller->ipc.call.owner != NULL) {
		hal_memcpy(caller->ipc.call.odata, _pendingSrc(caller), caller->ipc.call.osize);
		_unhold(caller);
	}
}


void xfer_flush(thread_t *owner)
{
	spinlock_ctx_t sc;
	thread_t *caller;

	/* only cleared concurrently, owner is the one to set it */
	if (__atomic_load_n(&owner->ipc.heldBy, __ATOMIC_ACQUIRE) == NULL) {
		return;
	}

	hal_spinlockSet(&threads_spinlock, &sc);
	caller = owner->ipc.heldBy;
	if (caller != NULL) {
		_xfer_copyToCaller(caller, caller->ipc.call.odata, _pendingSrc(caller), caller->ipc.call.osize);
		_unhold(caller);
	}
	hal_spinlockClear(&threads_spinlock, &sc);
}


void xfer_abort(thread_t *caller)
{
	spinlock_ctx_t sc;

	if (__atomic_exchange_n(&caller->ipc.call.active, 0U, __ATOMIC_ACQ_REL) != 0U) {
		_release(caller);
	}

	/* a dying caller is not switched to again, so its output is dropped */
	hal_spinlockSet(&threads_spinlock, &sc);
	if (caller->ipc.call.owner != NULL) {
		_unhold(caller);
	}
	hal_spinlockClear(&threads_spinlock, &sc);
}
