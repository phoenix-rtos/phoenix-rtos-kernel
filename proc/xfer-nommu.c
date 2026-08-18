#include "xfer.h"

#include "include/errno.h"

#define FLOOR(x) ((x) & ~(SIZE_PAGE - 1))
#define CEIL(x)  (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


static struct {
	vm_map_t *kmap;
} xfer_common;


int xfer_bufMap(process_t *src, process_t *dst, void *data, size_t size, xferBuf_t *xb, void **rdata)
{
	void *w = data;
	size_t sz;

	if ((data != NULL) && (size != 0U) && (dst != NULL) && (pmap_isAllowed(dst->pmapp, data, size) == 0)) {
		sz = round_page(size);
		w = vm_mmap(dst->mapp, NULL, NULL, sz, PROT_READ | PROT_USER, NULL, -1, MAP_ANONYMOUS);
		if (w == NULL) {
			return -ENOMEM;
		}
		hal_memcpy(w, data, size);
		*rdata = w;
		xb->w = w;
		xb->map = dst->mapp;
		xb->size = sz;
	}
	else {
		*rdata = data;
	}

	return EOK;
}


void xfer_bufRelease(xferBuf_t *xb)
{
	if (xb->w != NULL) {
		(void)vm_munmap(xb->map, xb->w, xb->size);
	}
}


int xfer_ipcBufBorrow(thread_t *from, thread_t *to)
{
	LIB_ASSERT(0, "TODO");
	return EOK;
}


void threads_releaseIpcBuf(thread_t *t)
{
}


void *proc_setupIpcBuf(thread_t *t, size_t sz)
{
	LIB_ASSERT(0, "TODO");
	return NULL;
}


void xfer_ipcBufRelease(thread_t *t)
{
}


xferPlan_t xfer_classify(thread_t *caller, thread_t *recv, const void *data, size_t size, size_t extraUsed)
{
	xferPlan_t plan = { msg_xfer_none, data, size, 0 };
	// void *w = caller->ipc.w;
	// size_t wsize = caller->ipc.size;

	if (size == 0) {
		return plan;
	}

	// if (size + extraUsed <= recv->ipc.size) {
	// 	plan.kind = msg_xfer_extra;
	// 	plan.ofs = extraUsed;
	// 	return plan;
	// }
	//
	// if (size <= wsize && data >= w && (const char *)data + size <= (const char *)w + wsize) {
	// 	plan.kind = msg_xfer_borrow;
	// 	plan.ofs = (const char *)data - (const char *)w;
	// 	return plan;
	// }

	plan.kind = msg_xfer_map;
	return plan;
}


int xfer_setup(thread_t *caller, thread_t *recv, const xferPlan_t *plan, xferBuf_t *xb, void **rdata, msg_side_t side)
{
	switch (plan->kind) {
		case msg_xfer_map:
			/* TODO: permissions, incoming data doesnt need to be writable */
			if (xfer_bufMap(caller->process, recv->process, (void *)plan->data, plan->size, xb, rdata) < 0) {
				xfer_bufRelease(xb);
				return -ENOMEM;
			}
			caller->ipc.flags |= (side == msg_side_in) ? MSG_IN_MAP : MSG_OUT_MAP;
			break;

		case msg_xfer_borrow:
			*rdata = caller->ipc.w + plan->ofs;
			break;

		case msg_xfer_extra:
			*rdata = recv->ipc.w + plan->ofs;
			caller->ipc.flags |= (side == msg_side_in) ? MSG_IN_EXTRA : MSG_OUT_EXTRA;
			break;

		default:
			LIB_ASSERT(plan->size == 0, "EEE");
			break;
	}

	return EOK;
}


static void _copyShadowPages(xferBuf_t *il, size_t size)
{
	if (il->bp != NULL) {
		hal_memcpy(il->bvaddr + il->boffs, il->w + il->boffs, min(SIZE_PAGE - il->boffs, size));
	}
	if (il->eoffs != 0) {
		size = min(size, il->size);
		hal_memcpy(il->evaddr, il->w + il->boffs + size - il->eoffs, il->eoffs);
	}
}


void xfer_copyShadowPages(thread_t *from, thread_t *to, msg_t *msg)
{
	if ((to->ipc.flags & MSG_OUT_MAP) != 0) {
		if (msg->o.size > 0) {
			_copyShadowPages(&to->ipc.obl, msg->o.size);
		}
	}
}


void xfer_init(vm_map_t *kmap)
{
	xfer_common.kmap = kmap;
}
