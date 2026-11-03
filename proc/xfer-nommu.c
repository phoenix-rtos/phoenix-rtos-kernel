/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Message transfer (NOMMU)
 *
 * Copyright 2026 Phoenix Systems
 * Author: Adam Greloch
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

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


void xfer_bufCopyBack(xferBuf_t *xb, size_t size)
{
	if ((xb->w != NULL) && (size > 0U)) {
		hal_memcpy(xb->orig, xb->w, size);
	}
}


/* The receiver may not be allowed to access the caller's buffer, so it gets a copy. */
int xfer_borrowMap(thread_t *caller, thread_t *recv)
{
	vm_map_t *map = (recv->process != NULL) ? recv->process->mapp : xfer_common.kmap;
	vm_prot_t prot = PROT_READ | PROT_WRITE;
	void *w;

	if (recv->process != NULL) {
		prot |= PROT_USER;
	}

	w = vm_mmap(map, NULL, NULL, caller->ipc.size, prot, NULL, -1, MAP_ANONYMOUS);
	if (w == NULL) {
		return -ENOMEM;
	}

	hal_memcpy(w, caller->ipc.w, caller->ipc.size);
	caller->ipc.call.bw = w;
	caller->ipc.call.bmap = map;

	return EOK;
}


void xfer_borrowCopyBack(thread_t *caller, size_t ofs, size_t size)
{
	if ((caller->ipc.call.bw != NULL) && (size > 0U)) {
		hal_memcpy((char *)caller->ipc.w + ofs, (char *)caller->ipc.call.bw + ofs, size);
	}
}


void xfer_borrowRelease(thread_t *caller)
{
	if (caller->ipc.call.bmap != NULL) {
		(void)vm_munmap(caller->ipc.call.bmap, caller->ipc.call.bw, caller->ipc.size);
	}

	caller->ipc.call.bw = NULL;
	caller->ipc.call.bmap = NULL;
}


void _xfer_copyToCaller(thread_t *caller, void *dst, const void *src, size_t size)
{
	(void)caller;

	if (size > 0U) {
		hal_memcpy(dst, src, size);
	}
}


void xfer_releaseIpcBuf(thread_t *t)
{
	xfer_flush(t);

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

		/* a call it serves may still have data in the buffer */
		if (t->reply != NULL) {
			return NULL;
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


void xfer_init(vm_map_t *kmap)
{
	xfer_common.kmap = kmap;
}
