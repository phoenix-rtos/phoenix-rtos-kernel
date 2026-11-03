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


void xfer_releaseIpcBuf(thread_t *t)
{
	(void)t;
}


void xfer_init(vm_map_t *kmap)
{
	xfer_common.kmap = kmap;
}
