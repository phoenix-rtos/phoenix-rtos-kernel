#include "xfer.h"
#include "include/errno.h"

#define FLOOR(x) ((x) & ~(SIZE_PAGE - 1))
#define CEIL(x)  (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


static struct {
	vm_map_t *kmap;
} xfer_common;


int xfer_bufMap(process_t *src, process_t *dst, void *data, size_t size, msg_side_t side, xferBuf_t *xb, void **rdata)
{
	void *w = data;
	size_t sz;
	vm_prot_t prot = PROT_READ | PROT_USER;

	xb->orig = data;

	if (side == msg_side_out) {
		prot |= PROT_WRITE;
	}

	if ((data != NULL) && (size != 0U) && (dst != NULL) && (pmap_isAllowed(dst->pmapp, data, size) == 0)) {
		sz = round_page(size);
		w = vm_mmap(dst->mapp, NULL, NULL, sz, prot, NULL, -1, MAP_ANONYMOUS);
		if (w == NULL) {
			return -ENOMEM;
		}
		xb->w = w;
		xb->map = dst->mapp;
		xb->size = sz;
		*rdata = w;
		if (side == msg_side_in) {
			hal_memcpy(w, data, size);
		}
	}
	else {
		*rdata = data;
		xb->w = NULL;
		xb->size = 0;
		xb->map = NULL;
	}

	return EOK;
}


void xfer_bufRelease(xferBuf_t *xb)
{
	if (xb->w != NULL) {
		(void)vm_munmap(xb->map, xb->w, xb->size);
	}

	xb->w = NULL;
	xb->orig = NULL;
	xb->size = 0;
	xb->map = NULL;
}


int xfer_ipcBufBorrow(thread_t *from, thread_t *to)
{
	vm_map_t *map = (to->process != NULL) ? to->process->mapp : xfer_common.kmap;
	vm_prot_t prot = PROT_READ | PROT_WRITE;
	void *buffer;

	if (from->ipc.w == NULL || from->ipc.size == 0U) {
		return -ENOMEM;
	}

	if (to->process != NULL) {
		prot |= PROT_USER;
	}

	buffer = vm_mmap(map, NULL, NULL, from->ipc.size, prot, NULL, -1, MAP_ANONYMOUS);
	if (buffer == NULL) {
		return -ENOMEM;
	}

	hal_memcpy(buffer, from->ipc.w, from->ipc.size);
	to->ipc.bw = buffer;
	to->ipc.bsize = from->ipc.size;

	return EOK;
}


void xfer_releaseIpcBuf(thread_t *t)
{
	vm_map_t *map;

	if (t->ipc.w != NULL) {
		map = (t->process != NULL) ? t->process->mapp : xfer_common.kmap;
		(void)vm_munmap(map, t->ipc.w, t->ipc.size);
		t->ipc.w = NULL;
		t->ipc.kw = NULL;
		t->ipc.size = 0;
	}
}


void *xfer_setupIpcBuf(thread_t *t, size_t sz)
{
	vm_map_t *map;
	vm_prot_t prot = PROT_READ | PROT_WRITE;
	void *buffer;

	if ((sz == 0U) || ((sz & (SIZE_PAGE - 1U)) != 0U)) {
		return NULL;
	}

	if (t->ipc.w != NULL) {
		if (t->ipc.size == sz) {
			return t->ipc.w;
		}

		xfer_releaseIpcBuf(t);
	}

	map = (t->process != NULL) ? t->process->mapp : xfer_common.kmap;
	if (t->process != NULL) {
		prot |= PROT_USER;
	}

	buffer = vm_mmap(map, NULL, NULL, sz, prot, NULL, -1, MAP_ANONYMOUS);
	if (buffer == NULL) {
		return NULL;
	}

	t->ipc.w = buffer;
	t->ipc.kw = buffer;
	t->ipc.size = sz;

	return buffer;
}


void xfer_ipcBufRelease(thread_t *t)
{
	if (t->ipc.bw != NULL) {
		vm_map_t *map = (t->process != NULL) ? t->process->mapp : xfer_common.kmap;

		(void)vm_munmap(map, t->ipc.bw, t->ipc.bsize);
		t->ipc.bw = NULL;
		t->ipc.bsize = 0;
	}
}


xferPlan_t xfer_classify(thread_t *caller, thread_t *recv, const void *data, size_t size, size_t extraUsed)
{
	xferPlan_t plan = { msg_xfer_none, data, size, 0 };
	void *w = caller->ipc.w;
	size_t wsize = caller->ipc.size;

	if (size == 0) {
		return plan;
	}

	if (size + extraUsed <= recv->ipc.size) {
		plan.kind = msg_xfer_extra;
		plan.ofs = extraUsed;
		return plan;
	}

	if (size <= wsize && data >= w && (const char *)data + size <= (const char *)w + wsize) {
		plan.kind = msg_xfer_borrow;
		plan.ofs = (const char *)data - (const char *)w;
		return plan;
	}

	plan.kind = msg_xfer_map;
	return plan;
}


int xfer_setup(thread_t *caller, thread_t *recv, const xferPlan_t *plan, xferBuf_t *xb, void **rdata, msg_side_t side)
{
	switch (plan->kind) {
		case msg_xfer_map:
			if (xfer_bufMap(caller->process, recv->process, (void *)plan->data, plan->size, side, xb, rdata) < 0) {
				xfer_bufRelease(xb);
				return -ENOMEM;
			}
			caller->ipc.flags |= (side == msg_side_in) ? MSG_IN_MAP : MSG_OUT_MAP;
			break;

		case msg_xfer_borrow:
			LIB_ASSERT(recv->ipc.bw != NULL, "borrowed IPC buffer missing");
			*rdata = (char *)recv->ipc.bw + plan->ofs;
			break;

		case msg_xfer_extra:
			LIB_ASSERT(recv->ipc.w != NULL, "receiver IPC buffer missing");
			*rdata = (char *)recv->ipc.w + plan->ofs;
			caller->ipc.flags |= (side == msg_side_in) ? MSG_IN_EXTRA : MSG_OUT_EXTRA;
			break;

		default:
			LIB_ASSERT(plan->size == 0, "EEE");
			break;
	}

	return EOK;
}


/* TODO: API is kinda bad. make it clear what should happen here. */
void xfer_finalize(thread_t *from, thread_t *to, msg_t *msg)
{
	if (((to->ipc.flags & MSG_OUT_MAP) != 0) && (to->ipc.obl.w != NULL)) {
		if (to->ipc.msgPtr->o.size > 0U) {
			hal_memcpy(to->ipc.obl.orig, to->ipc.obl.w, to->ipc.msgPtr->o.size);
		}
	}

	if (from->ipc.bw != NULL && msg->o.data >= from->ipc.bw &&
			msg->o.data < (void *)((char *)from->ipc.bw + from->ipc.bsize)) {
		size_t offset = (char *)msg->o.data - (char *)from->ipc.bw;

		if (msg->o.size <= from->ipc.bsize - offset) {
			hal_memcpy((char *)to->ipc.w + offset, msg->o.data, msg->o.size);
		}
	}
}


void xfer_init(vm_map_t *kmap)
{
	xfer_common.kmap = kmap;
}
