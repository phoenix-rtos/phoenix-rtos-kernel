#include "xfer.h"

#include "include/errno.h"

#define FLOOR(x) ((x) & ~(SIZE_PAGE - 1))
#define CEIL(x)  (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


static struct {
	vm_map_t *kmap;
} xfer_common;


static vm_map_t *_getMap(process_t *process)
{
	return (process == NULL || process->mapp == NULL) ? xfer_common.kmap : process->mapp;
}


static vm_flags_t _getMapFlags(vm_map_t *map, void *data)
{
	if (pmap_belongs(&map->pmap, data) != 0) {
		return vm_mapFlags(map, data);
	}
	else {
		return vm_mapFlags(xfer_common.kmap, data);
	}
}


static int _mapBoundaryPage(vm_map_t *srcmap, vm_map_t *dstmap, void *src, void *dst, size_t srcoffs, size_t dstoffs, size_t size, page_t *page, vm_attr_t attr, vm_flags_t flags, void **vaddr)
{
	addr_t paddr;

	paddr = pmap_resolve(&srcmap->pmap, src) & ~(SIZE_PAGE - 1);
	if (paddr == 0) {
		return -ENOMEM;
	}

	*vaddr = vm_mmap(xfer_common.kmap, NULL, NULL, SIZE_PAGE, PROT_READ | PROT_WRITE, VM_OBJ_PHYSMEM, paddr, flags);
	if (*vaddr == NULL) {
		return -ENOMEM;
	}

	if (page_map(&dstmap->pmap, dst, page->addr, (attr | PGHD_WRITE) & ~PGHD_USER) < 0) {
		return -ENOMEM;
	}

	hal_memcpy((char *)dst + dstoffs, (char *)*vaddr + srcoffs, size);

	if (page_map(&dstmap->pmap, dst, page->addr, attr) < 0) {
		return -ENOMEM;
	}

	return EOK;
}


int xfer_bufMap(process_t *src, process_t *dst, void *data, size_t size, msg_side_t side, xferBuf_t *xb, void **rdata)
{
	void *w = NULL, *vaddr;
	u64 boffs, eoffs;
	unsigned int n = 0, i;
	page_t *nep = NULL, *nbp = NULL;
	vm_map_t *srcmap, *dstmap;
	int flags;
	addr_t pa;

	if ((size == 0) || (data == NULL)) {
		return -EINVAL;
	}

	unsigned int attr = PGHD_WRITE | PGHD_READ | PGHD_PRESENT | PGHD_USER;
	vm_prot_t prot = PROT_READ | PROT_USER;
	if (side == msg_side_out) {
		prot |= PROT_WRITE;
	}

	boffs = (ptr_t)data & (SIZE_PAGE - 1);

	if (FLOOR((ptr_t)data + size) > CEIL((ptr_t)data)) {
		n = (FLOOR((ptr_t)data + size) - CEIL((ptr_t)data)) / SIZE_PAGE;
	}

	if ((boffs != 0) && (FLOOR((ptr_t)data) == FLOOR((ptr_t)data + size))) {
		/* Data is on one page only and will be copied by boffs handler */
		eoffs = 0;
	}
	else {
		eoffs = ((ptr_t)data + size) & (SIZE_PAGE - 1);
	}

	srcmap = _getMap(src);
	dstmap = _getMap(dst);

	if ((srcmap == dstmap) && (pmap_belongs(&dstmap->pmap, data) != 0)) {
		*rdata = data;
		return EOK;
	}

	size_t sz = (((boffs != 0) ? 1 : 0) + ((eoffs != 0) ? 1 : 0) + n) * SIZE_PAGE;
	w = vm_mapFind(dstmap, NULL, sz, MAP_NOINHERIT, prot);
	xb->w = w;
	if (w == NULL) {
		return -ENOMEM;
	}
	xb->size = sz;
	xb->map = dstmap;

	flags = _getMapFlags(srcmap, data);
	if (flags < 0) {
		return -EINVAL;
	}

	attr |= vm_flagsToAttr(flags);

	if (boffs > 0) {
		xb->boffs = boffs;
		nbp = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
		xb->bp = nbp;
		if (nbp == NULL) {
			return -ENOMEM;
		}

		if (_mapBoundaryPage(srcmap, dstmap, data, w, boffs, boffs, min(size, SIZE_PAGE - boffs), nbp, attr, flags, &xb->bvaddr) < 0) {
			return -ENOMEM;
		}
	}

	/* Map pages */
	vaddr = (void *)CEIL((ptr_t)data);

	for (i = 0; i < n; i++, vaddr += SIZE_PAGE) {
		pa = pmap_resolve(&srcmap->pmap, vaddr) & ~(SIZE_PAGE - 1);
		if (page_map(&dstmap->pmap, w + (i + ((boffs != 0) ? 1 : 0)) * SIZE_PAGE, pa, attr) < 0) {
			return -ENOMEM;
		}
	}

	if (eoffs) {
		xb->eoffs = eoffs;
		vaddr = (void *)FLOOR((ptr_t)data + size);

		if ((boffs == 0) || (eoffs >= boffs)) {
			nep = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
			xb->ep = nep;
			if (nep == NULL) {
				return -ENOMEM;
			}
		}
		else {
			nep = nbp;
		}

		if (_mapBoundaryPage(srcmap, dstmap, vaddr, w + (n + ((boffs != 0) ? 1 : 0)) * SIZE_PAGE, 0, 0, eoffs, nep, attr, flags, &xb->evaddr) < 0) {
			return -ENOMEM;
		}
	}

	*rdata = (w + boffs);

	return EOK;
}


void xfer_bufRelease(xferBuf_t *xb)
{
	if (xb->bp != NULL) {
		vm_pageFree(xb->bp);
		vm_munmap(xfer_common.kmap, xb->bvaddr, SIZE_PAGE);
		xb->bp = NULL;
	}

	if (xb->eoffs != 0) {
		if (xb->ep != NULL) {
			vm_pageFree(xb->ep);
		}
		vm_munmap(xfer_common.kmap, xb->evaddr, SIZE_PAGE);
		xb->eoffs = 0;
		xb->ep = NULL;
	}

	if (xb->w != NULL) {
		vm_munmap(xb->map, xb->w, xb->size);
		xb->w = NULL;
		xb->size = 0;
		xb->map = NULL;
	}
}


int xfer_ipcBufBorrow(thread_t *from, thread_t *to)
{
	vm_map_t *dstmap = _getMap(to->process);

	vm_flags_t flags = MAP_NOINHERIT;
	vm_attr_t attr = PGHD_READ | PGHD_WRITE | PGHD_PRESENT | vm_flagsToAttr(flags);
	vm_prot_t prot = PROT_WRITE | PROT_READ;

	if (to->process != NULL) {
		attr |= PGHD_USER;
		prot |= PROT_USER;
	}

	/* TODO: find only for payload size not whole buf */
	/* TODO: this doesn't handle non-contigous >1page buffers */
	void *vaddr = vm_mapFind(dstmap, NULL, from->ipc.size, flags, prot);
	if (vaddr == NULL) {
		return -ENOMEM;
	}

	if (page_map(&dstmap->pmap, vaddr, from->ipc.p->addr, attr) < 0) {
		return -ENOMEM;
	}

	to->ipc.bw = vaddr;
	to->ipc.bsize = from->ipc.size;

	return EOK;
}

void xfer_releaseIpcBuf(thread_t *t)
{
	if (t->ipc.w != NULL) {
		if (t->process != NULL) {
			vm_munmap(&t->process->map, t->ipc.w, t->ipc.size);
		}
		t->ipc.w = NULL;
	}
	if (t->ipc.kw != NULL) {
		vm_munmap(xfer_common.kmap, t->ipc.kw, t->ipc.size);
		t->ipc.kw = NULL;
	}
	if (t->ipc.p != NULL) {
		page_t *p = t->ipc.p;
		page_t *next = p;
		while (p != NULL) {
			next = p->next;
			vm_pageFree(p);
			p = next;
		}
		t->ipc.p = NULL;
	}
}


void *xfer_setupIpcBuf(thread_t *t, size_t sz)
{
	void *vaddr = NULL, *kvaddr = NULL;
	vm_map_t *map;
	page_t *p;
	u8 prot, flags, attr;

	if (sz == 0 || sz > MSG_MAX_IPC_BUF || (sz & (SIZE_PAGE - 1)) != 0) {
		return NULL;
	}

	if (t->ipc.kw != NULL) {
		if (t->ipc.size == sz) {
			LIB_ASSERT(t->ipc.w != NULL, "");
			LIB_ASSERT(t->ipc.p != NULL, "");
			return t->ipc.w;
		}
		else {
			xfer_releaseIpcBuf(t);
		}
	}

	map = _getMap(t->process);

	prot = PROT_WRITE | PROT_READ;
	flags = MAP_NOINHERIT;
	attr = PGHD_READ | PGHD_WRITE | PGHD_PRESENT | vm_flagsToAttr(flags);

	/* map to kernel space */
	kvaddr = vm_mapFind(xfer_common.kmap, NULL, sz, flags, prot);
	if (kvaddr == NULL) {
		xfer_releaseIpcBuf(t);
		return NULL;
	}
	t->ipc.kw = kvaddr;

	if (t->process != NULL) {
		/* map to current thread space */
		vaddr = vm_mapFind(map, NULL, sz, flags, prot | PROT_USER);
		if (vaddr == NULL) {
			xfer_releaseIpcBuf(t);
			return NULL;
		}
		t->ipc.w = vaddr;
	}
	else {
		/* this is a kernel thread, so t->utcb.w is already mapped to its space */
		t->ipc.w = t->ipc.kw;
	}

	size_t ofs;
	for (ofs = 0; ofs < sz; ofs += SIZE_PAGE) {
		p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);

		if (p == NULL) {
			xfer_releaseIpcBuf(t);
			return NULL;
		}

		p->next = t->ipc.p;
		t->ipc.p = p;

		if (page_map(&xfer_common.kmap->pmap, kvaddr + ofs, p->addr, attr) < 0) {
			xfer_releaseIpcBuf(t);
			return NULL;
		}

		if (t->process != NULL && page_map(&map->pmap, vaddr + ofs, p->addr, attr | PGHD_USER) < 0) {
			xfer_releaseIpcBuf(t);
			return NULL;
		}
	}

	t->ipc.size = sz;

	return t->ipc.w;
}


void xfer_ipcBufRelease(thread_t *t)
{
	if (t->ipc.bw != NULL) {
		vm_munmap(_getMap(t->process), t->ipc.bw, t->ipc.bsize);
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


void xfer_finalize(thread_t *from, thread_t *to, msg_t *msg)
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
