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


/*
 * Boundary-page aliases live in a kmap window reserved once per xferBuf_t.
 * Re-pointing a slot is a single page_map() - a PTE store plus a TLB
 * invalidate - to avoid calling vm_mmap().
 */
#define XFER_WIN_BSRC  0U /* caller's head page */
#define XFER_WIN_BDST  1U /* shadow page substituted for it */
#define XFER_WIN_ESRC  2U /* caller's tail page */
#define XFER_WIN_EDST  3U /* shadow page substituted for it */
#define XFER_WIN_SLOTS 4U
#define XFER_WIN_SIZE  (XFER_WIN_SLOTS * SIZE_PAGE)


static void *_winSlot(const xferBuf_t *xb, unsigned int slot)
{
	return (char *)xb->win + (slot * SIZE_PAGE);
}


/* Head and tail fragments may share one shadow page - see xfer_bufMap(). */
static void *_winShadowE(const xferBuf_t *xb)
{
	return _winSlot(xb, (xb->ep != NULL) ? XFER_WIN_EDST : XFER_WIN_BDST);
}


static int _winReserve(xferBuf_t *xb)
{
	if (xb->win == NULL) {
		xb->win = vm_mapFind(xfer_common.kmap, NULL, XFER_WIN_SIZE, MAP_NOINHERIT, PROT_READ | PROT_WRITE);
		if (xb->win == NULL) {
			return -ENOMEM;
		}
	}

	return EOK;
}


static void _winRelease(xferBuf_t *xb)
{
	if (xb->win != NULL) {
		vm_munmap(xfer_common.kmap, xb->win, XFER_WIN_SIZE);
		xb->win = NULL;
	}
}


static int _mapBoundaryPage(vm_map_t *srcmap, void *src, size_t srcoffs, page_t *page, size_t dstoffs, size_t size, vm_attr_t kattr, void *sslot, void *dslot)
{
	addr_t paddr;

	paddr = pmap_resolve(&srcmap->pmap, src) & ~(SIZE_PAGE - 1);
	if (paddr == 0) {
		return -ENOMEM;
	}

	if (page_map(&xfer_common.kmap->pmap, sslot, paddr, kattr) < 0) {
		return -ENOMEM;
	}

	if (page_map(&xfer_common.kmap->pmap, dslot, page->addr, kattr) < 0) {
		return -ENOMEM;
	}

	hal_memcpy((char *)dslot + dstoffs, (char *)sslot + srcoffs, size);

	return EOK;
}


int xfer_bufMap(process_t *src, process_t *dst, void *data, size_t size, msg_side_t side, xferBuf_t *xb, void **rdata)
{
	void *w = NULL, *vaddr, *epage;
	vm_attr_t kattr;
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

	kattr = PGHD_PRESENT | PGHD_READ | PGHD_WRITE | vm_flagsToAttr(flags);

	if (((boffs != 0) || (eoffs != 0)) && (_winReserve(xb) < 0)) {
		return -ENOMEM;
	}

	if (boffs > 0) {
		xb->boffs = boffs;
		nbp = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
		xb->bp = nbp;
		if (nbp == NULL) {
			return -ENOMEM;
		}

		if (page_map(&dstmap->pmap, w, nbp->addr, attr) < 0) {
			return -ENOMEM;
		}

		if (_mapBoundaryPage(srcmap, data, boffs, nbp, boffs, min(size, SIZE_PAGE - boffs), kattr,
					_winSlot(xb, XFER_WIN_BSRC), _winSlot(xb, XFER_WIN_BDST)) < 0) {
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
		epage = w + (n + ((boffs != 0) ? 1 : 0)) * SIZE_PAGE;

		if ((boffs == 0) || (eoffs >= boffs)) {
			nep = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
			xb->ep = nep;
			if (nep == NULL) {
				return -ENOMEM;
			}
		}
		else {
			/*
			 * Head/tail fragments share one shadow page, mapped twice so
			 * they appear contiguous across the window boundary.
			 */
			nep = nbp;
			xb->ep = NULL;
		}

		if (page_map(&dstmap->pmap, epage, nep->addr, attr) < 0) {
			return -ENOMEM;
		}

		if (_mapBoundaryPage(srcmap, vaddr, 0, nep, 0, eoffs, kattr,
					_winSlot(xb, XFER_WIN_ESRC), _winShadowE(xb)) < 0) {
			return -ENOMEM;
		}
	}

	*rdata = (w + boffs);

	return EOK;
}


void xfer_bufRelease(xferBuf_t *xb)
{
	/* The aliases must go before the pages below are recycled to prevent UAF. */
	if (xb->bp != NULL) {
		(void)pmap_remove(&xfer_common.kmap->pmap, _winSlot(xb, XFER_WIN_BSRC),
				(char *)_winSlot(xb, XFER_WIN_BDST) + SIZE_PAGE);
	}

	if (xb->eoffs != 0) {
		(void)pmap_remove(&xfer_common.kmap->pmap, _winSlot(xb, XFER_WIN_ESRC),
				(char *)_winSlot(xb, XFER_WIN_EDST) + SIZE_PAGE);
	}

	if (xb->bp != NULL) {
		vm_pageFree(xb->bp);
		xb->bp = NULL;
	}

	/* NULL whenever the tail shares the head's shadow page - freed above */
	if (xb->ep != NULL) {
		vm_pageFree(xb->ep);
		xb->ep = NULL;
	}

	xb->boffs = 0;
	xb->eoffs = 0;

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

	__atomic_exchange_n(&to->ipc.bsize, from->ipc.size, __ATOMIC_RELAXED);
	__atomic_store_n(&to->ipc.bw, vaddr, __ATOMIC_RELEASE);

	return EOK;
}

void xfer_releaseIpcBuf(thread_t *t)
{
	_winRelease(&t->ipc.ibl);
	_winRelease(&t->ipc.obl);

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
	vm_prot_t prot;
	vm_flags_t flags;
	vm_attr_t attr;

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
	void *bw = __atomic_exchange_n(&t->ipc.bw, NULL, __ATOMIC_ACQUIRE);
	size_t bsize = __atomic_exchange_n(&t->ipc.bsize, 0, __ATOMIC_RELAXED);

	if (bw != NULL) {
		vm_munmap(_getMap(t->process), bw, bsize);
		bsize = 0;
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
		hal_memcpy((char *)_winSlot(il, XFER_WIN_BSRC) + il->boffs,
				(char *)_winSlot(il, XFER_WIN_BDST) + il->boffs,
				min(SIZE_PAGE - il->boffs, size));
	}
	if (il->eoffs != 0) {
		/*
		 * The tail fragment always sits at offset 0 of the tail shadow page.
		 * That is where _mapBoundaryPage() wrote it (dstoffs == 0).
		 */
		hal_memcpy(_winSlot(il, XFER_WIN_ESRC), _winShadowE(il), il->eoffs);
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
