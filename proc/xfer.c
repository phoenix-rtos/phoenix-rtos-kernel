/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Message transfer (MMU)
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


#define WIN_PAGE(xb, i) ((char *)(xb)->win + ((i) * SIZE_PAGE))
#define WIN_BSHADOW(xb) WIN_PAGE(xb, 0U)
#define WIN_ESHADOW(xb) WIN_PAGE(xb, 1U)
#define WIN_BCALLER(xb) WIN_PAGE(xb, 2U) /* output only */
#define WIN_ECALLER(xb) WIN_PAGE(xb, 3U) /* output only */
#define WIN_SIZE        (4U * SIZE_PAGE)


static struct {
	vm_map_t *kmap;
} xfer_common;


static vm_map_t *_getMap(process_t *process)
{
	return ((process == NULL) || (process->mapp == NULL)) ? xfer_common.kmap : process->mapp;
}


/* Maps the page src has at sva to dva in dst. */
static int _mapAlias(pmap_t *dst, void *dva, vm_map_t *src, ptr_t sva, vm_attr_t attr)
{
	addr_t pa = FLOOR(pmap_resolve(&src->pmap, (void *)sva));

	return ((pa == 0U) || (page_map(dst, dva, pa, attr) < 0)) ? -ENOMEM : EOK;
}


int xfer_bufMap(process_t *src, process_t *dst, void *data, size_t size, msg_side_t side, xferBuf_t *xb, void **rdata)
{
	vm_map_t *srcmap = _getMap(src), *dstmap = _getMap(dst);
	pmap_t *kpmap = &xfer_common.kmap->pmap;
	ptr_t start = (ptr_t)data, end = start + size, va;
	size_t boffs = start & (SIZE_PAGE - 1U), eoffs;
	vm_attr_t attr = PGHD_PRESENT | PGHD_READ, kattr;
	vm_prot_t prot = PROT_READ;
	int flags;
	char *w;

	if ((size == 0U) || (data == NULL)) {
		return -EINVAL;
	}

	if ((srcmap == dstmap) && (pmap_belongs(&dstmap->pmap, data) != 0)) {
		*rdata = data;
		return EOK;
	}

	/* kernel data sent by a process is not in its map */
	flags = vm_mapFlags((pmap_belongs(&srcmap->pmap, data) != 0) ? srcmap : xfer_common.kmap, data);
	if (flags < 0) {
		return -EINVAL;
	}

	attr |= vm_flagsToAttr((vm_flags_t)flags);
	kattr = attr | PGHD_WRITE;
	if (side == msg_side_out) {
		attr |= PGHD_WRITE;
		prot |= PROT_WRITE;
	}
	if (dst != NULL) {
		attr |= PGHD_USER;
		prot |= PROT_USER;
	}

	xb->size = CEIL(end) - FLOOR(start);
	xb->map = dstmap;
	xb->w = vm_mapFind(dstmap, NULL, xb->size, MAP_NOINHERIT, prot);
	if (xb->w == NULL) {
		return -ENOMEM;
	}
	w = xb->w;

	/* data ending in its head page is all in the head shadow */
	eoffs = (FLOOR(end) >= CEIL(start)) ? (end & (SIZE_PAGE - 1U)) : 0U;
	xb->boffs = boffs;
	xb->eoffs = eoffs;

	if (((boffs != 0U) || (eoffs != 0U)) && (xb->win == NULL)) {
		xb->win = vm_mapFind(xfer_common.kmap, NULL, WIN_SIZE, MAP_NOINHERIT, PROT_READ | PROT_WRITE);
		if (xb->win == NULL) {
			return -ENOMEM;
		}
	}

	/* we run in the caller's address space, so the data is copied in directly */
	if (boffs != 0U) {
		xb->bp = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
		if ((xb->bp == NULL) || (page_map(kpmap, WIN_BSHADOW(xb), xb->bp->addr, kattr) < 0)) {
			return -ENOMEM;
		}
		hal_memcpy(WIN_BSHADOW(xb) + boffs, data, min(size, SIZE_PAGE - boffs));
	}

	if (eoffs != 0U) {
		/* the fragments do not overlap, so one shadow can hold both */
		xb->ep = (eoffs <= boffs) ? xb->bp : vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
		if ((xb->ep == NULL) || (page_map(kpmap, WIN_ESHADOW(xb), xb->ep->addr, kattr) < 0)) {
			return -ENOMEM;
		}
		hal_memcpy(WIN_ESHADOW(xb), (void *)FLOOR(end), eoffs);
	}

	if (side == msg_side_out) {
		if ((boffs != 0U) && (_mapAlias(kpmap, WIN_BCALLER(xb), srcmap, FLOOR(start), kattr) < 0)) {
			return -ENOMEM;
		}
		if ((eoffs != 0U) && (_mapAlias(kpmap, WIN_ECALLER(xb), srcmap, FLOOR(end), kattr) < 0)) {
			return -ENOMEM;
		}
	}

	if ((xb->bp != NULL) && (page_map(&dstmap->pmap, w, xb->bp->addr, attr) < 0)) {
		return -ENOMEM;
	}
	for (va = CEIL(start); va < FLOOR(end); va += SIZE_PAGE) {
		if (_mapAlias(&dstmap->pmap, w + (va - FLOOR(start)), srcmap, va, attr) < 0) {
			return -ENOMEM;
		}
	}
	if ((xb->ep != NULL) && (page_map(&dstmap->pmap, w + (FLOOR(end) - FLOOR(start)), xb->ep->addr, attr) < 0)) {
		return -ENOMEM;
	}

	*rdata = w + boffs;

	return EOK;
}


void xfer_bufCopyBack(xferBuf_t *xb, size_t size)
{
	if (xb->bp != NULL) {
		hal_memcpy(WIN_BCALLER(xb) + xb->boffs, WIN_BSHADOW(xb) + xb->boffs, min(size, SIZE_PAGE - xb->boffs));
	}
	if (xb->ep != NULL) {
		hal_memcpy(WIN_ECALLER(xb), WIN_ESHADOW(xb), xb->eoffs);
	}
}


void xfer_bufRelease(xferBuf_t *xb)
{
	if (xb->w == NULL) {
		return;
	}

	/* nothing may map the shadows once they are freed */
	(void)vm_munmap(xb->map, xb->w, xb->size);
	if ((xb->bp != NULL) || (xb->ep != NULL)) {
		(void)pmap_remove(&xfer_common.kmap->pmap, xb->win, (char *)xb->win + WIN_SIZE);
	}

	if (xb->bp != NULL) {
		vm_pageFree(xb->bp);
	}
	if ((xb->ep != NULL) && (xb->ep != xb->bp)) {
		vm_pageFree(xb->ep);
	}

	xb->w = NULL;
	xb->size = 0;
	xb->map = NULL;
	xb->boffs = 0;
	xb->bp = NULL;
	xb->eoffs = 0;
	xb->ep = NULL;
}


static void _winRelease(xferBuf_t *xb)
{
	if (xb->win != NULL) {
		(void)vm_munmap(xfer_common.kmap, xb->win, WIN_SIZE);
		xb->win = NULL;
	}
}


void xfer_releaseIpcBuf(thread_t *t)
{
	_winRelease(&t->ipc.call.ibl);
	_winRelease(&t->ipc.call.obl);
}


void xfer_init(vm_map_t *kmap)
{
	xfer_common.kmap = kmap;
}
