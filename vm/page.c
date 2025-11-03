/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - page allocator
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "lib/lib.h"
#include "proc/proc.h"
#include "include/errno.h"
#include "include/mman.h"
#include "page.h"


#define SIZE_VM_SIZES 32


struct {
	page_t *sizes[SIZE_VM_SIZES];
	page_t *pages;

	size_t allocsz;
	size_t bootsz;
	size_t freesz;

	lock_t lock;
} pages;


static page_t *_page_alloc(size_t size, vm_flags_t flags)
{
	unsigned int start, stop, i;
	page_t *lh, *rh;

	/* Establish first index */
	size = (size < SIZE_PAGE) ? SIZE_PAGE : size;

	start = hal_cpuGetLastBit(size);
	if (hal_cpuGetFirstBit(size) < start) {
		start++;
	}

	/* Find segment */
	stop = start;

	while ((stop < SIZE_VM_SIZES) && (pages.sizes[stop] == NULL)) {
		stop++;
	}
	if (stop == SIZE_VM_SIZES) {
		return NULL;
	}

	lh = pages.sizes[stop];

	/* Split segment */
	while (stop > start) {
		LIST_REMOVE(&pages.sizes[stop], lh);

		stop--;

		lh->idx--;
		rh = lh + (1 << lh->idx) / SIZE_PAGE;
		rh->idx = lh->idx;
		LIST_ADD(&pages.sizes[stop], lh);
		LIST_ADD(&pages.sizes[stop], rh);
	}

	LIST_REMOVE(&pages.sizes[stop], lh);

	/* Mark allocated pages */
	for (i = 0; i < (1 << lh->idx) / SIZE_PAGE; i++) {
		(lh + i)->flags &= ~PAGE_FREE;
		(lh + i)->flags |= flags;
		pages.freesz -= SIZE_PAGE;
		pages.allocsz += SIZE_PAGE;
	}

	return lh;
}


page_t *vm_pageAlloc(size_t size, vm_flags_t flags)
{
	page_t *p;

	proc_lockSet(&pages.lock);
	p = _page_alloc(size, flags);
	proc_lockClear(&pages.lock);
	return p;
}


void vm_pageFree(page_t *p)
{
	unsigned int idx, i;
	page_t *lh = p, *rh = p;

	proc_lockSet(&pages.lock);

	if ((lh->flags & PAGE_FREE) != 0) {
		hal_cpuDisableInterrupts();
		lib_printf("page: double free (%p)\n", lh);
		hal_cpuEnableInterrupts();
		for (;;) {
		}
	}

	idx = p->idx;

	/* Mark free pages */
	for (i = 0; i < (1 << idx) / SIZE_PAGE; i++) {
		(p + i)->flags |= PAGE_FREE;
		pages.freesz += SIZE_PAGE;
		pages.allocsz -= SIZE_PAGE;
	}

	if ((p->addr & ((1 << (idx + 1)) - 1)) != 0) {
		lh = p - (1 << idx) / SIZE_PAGE;
	}
	else {
		rh = p + (1 << idx) / SIZE_PAGE;
	}

	while ((lh >= pages.pages) && (rh < (pages.pages + (pages.allocsz + pages.freesz) / SIZE_PAGE)) && ((lh->flags & PAGE_FREE) != 0) && ((rh->flags & PAGE_FREE) != 0) && (lh->idx == rh->idx) && ((lh->addr + (1 << lh->idx)) == rh->addr) && (idx < SIZE_VM_SIZES)) {

		if (p == lh) {
			LIST_REMOVE(&pages.sizes[idx], rh);
		}
		else {
			LIST_REMOVE(&pages.sizes[idx], lh);
		}

		rh->idx = hal_cpuGetFirstBit(SIZE_PAGE);
		lh->idx++;
		idx++;

		p = lh;

		if ((p->addr & ((1 << (idx + 1)) - 1)) != 0) {
			lh = p - (1 << idx) / SIZE_PAGE;
		}
		else {
			rh = p + (1 << idx) / SIZE_PAGE;
		}
	}

	LIST_ADD(&pages.sizes[idx], p);

	proc_lockClear(&pages.lock);
	return;
}


static int _page_get_cmp(void *key, void *item)
{
	addr_t a = (addr_t)key;
	page_t *p = (page_t *)item;

	if (a == p->addr) {
		return 0;
	}

	if (a > p->addr) {
		return 1;
	}

	return -1;
}


page_t *_page_get(addr_t addr)
{
	page_t *p;
	size_t np = (pages.freesz + pages.allocsz) / SIZE_PAGE;

	addr = addr & ~(SIZE_PAGE - 1);
	p = lib_bsearch((void *)addr, pages.pages, np, sizeof(page_t), _page_get_cmp);

	return p;
}


static void _page_initSizes(void)
{
	unsigned int i, k, idx;
	page_t *p;

	/* Remove already discovered pages */
	pages.sizes[hal_cpuGetFirstBit(SIZE_PAGE)] = NULL;

	for (i = 0; i < (pages.allocsz + pages.freesz) / SIZE_PAGE;) {
		p = &pages.pages[i];
		if ((p->flags & PAGE_FREE) == 0) {
			i++;
			continue;
		}

		idx = hal_cpuGetFirstBit(p->addr);

		if (idx >= SIZE_VM_SIZES) {
			idx = SIZE_VM_SIZES - 1;
		}

		for (k = 0; (k < ((1 << idx) / SIZE_PAGE) - 1) && ((i + k) < (((pages.allocsz + pages.freesz) / SIZE_PAGE) - 1)); k++) {
			if ((pages.pages[i + 1 + k].flags & PAGE_FREE) == 0) {
				break;
			}
		}

		idx = hal_cpuGetLastBit((1 + k) * SIZE_PAGE);
		p->idx = idx;

		LIST_ADD(&pages.sizes[idx], p);

		i += ((1UL << idx) / SIZE_PAGE);
	}
	return;
}


static unsigned int page_digits(unsigned int n, unsigned int base)
{
	unsigned int d = 1;

	while ((n /= base) != 0) {
		d++;
	}

	return d;
}


#define TTY_COLS 80
void _page_showPages(void)
{
	addr_t a;
	page_t *p;
	unsigned int rep, i, k, w;
	char c;
	char buf[TTY_COLS + 1];

	w = lib_sprintf(buf, "vm: ");
	for (i = 0, a = 0; i < (pages.freesz + pages.allocsz) / SIZE_PAGE; i++) {
		p = &pages.pages[i];

		/* Print markers in case of memory gap */
		if (p->addr > a) {
			rep = (p->addr - a) / SIZE_PAGE;
			if (rep >= 4) {
				k = page_digits(rep, 10) + 3;
				if (w + k > TTY_COLS) {
					lib_printf("%s\n", buf);
					w = lib_sprintf(buf, "vm: ");
				}
				w += lib_sprintf(buf + w, "[%dx]", rep);
			}
			else {
				for (k = 0; k < rep; k++) {
					if (w + 1 > TTY_COLS) {
						lib_printf("%s\n", buf);
						w = lib_sprintf(buf, "vm: ");
					}
					w += lib_sprintf(buf + w, "%c", 'x');
				}
			}
		}

		/* Print markers with repetitions */
		c = pmap_marker(p);
		for (rep = 0; (i + rep + 1) < (pages.freesz + pages.allocsz) / SIZE_PAGE; rep++) {
			if ((c != pmap_marker(&pages.pages[i + rep + 1])) || (pages.pages[i + rep + 1].addr - pages.pages[i + rep].addr > SIZE_PAGE)) {
				break;
			}
		}

		if (rep >= 4) {
			k = page_digits(rep + 1, 10) + 3;
			if (w + k > TTY_COLS) {
				lib_printf("%s\n", buf);
				w = lib_sprintf(buf, "vm: ");
			}
			w += lib_sprintf(buf + w, "[%d%c]", rep + 1, c);
		}
		else {
			for (k = 0; k <= rep; k++) {
				if (w + 1 > TTY_COLS) {
					lib_printf("%s\n", buf);
					w = lib_sprintf(buf, "vm: ");
				}
				w += lib_sprintf(buf + w, "%c", pmap_marker(p));
			}
		}

		a = pages.pages[i + rep].addr + SIZE_PAGE;
		i += rep;
	}

	if (w > 4) {
		lib_printf("%s\n", buf);
	}

	return;
}


static int _page_map(pmap_t *pmap, void *vaddr, addr_t pa, vm_attr_t attrs)
{
	page_t *ap = NULL;

	while (pmap_enter(pmap, pa, vaddr, attrs, ap) < 0) {
		ap = _page_alloc(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE);
		if (/*vaddr > (void *)VADDR_KERNEL ||*/ ap == NULL) {
			return -ENOMEM;
		}
	}
	return EOK;
}


int page_map(pmap_t *pmap, void *vaddr, addr_t pa, vm_attr_t attrs)
{
	int err;

	proc_lockSet(&pages.lock);
	err = _page_map(pmap, vaddr, pa, attrs);
	proc_lockClear(&pages.lock);

	return err;
}


int _page_sbrk(pmap_t *pmap, void **start, void **end)
{
	page_t *np, *ap = NULL;
	np = _page_alloc(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP);
	if (np == NULL) {
		return -ENOMEM;
	}

	while (pmap_enter(pmap, np->addr, (*end), PGHD_READ | PGHD_WRITE | PGHD_PRESENT, ap) < 0) {
		ap = _page_alloc(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE);
		if (ap == NULL) {
			return -ENOMEM;
		}
	}

	(*end) += SIZE_PAGE;

	return EOK;
}


void vm_pageGetStats(size_t *freesz)
{
	*freesz = pages.freesz;
}


void vm_pageinfo(meminfo_t *info)
{
	char c;
	page_t *p;
	unsigned int size, rep, i;

	proc_lockSet(&pages.lock);

	info->page.alloc = pages.allocsz;
	info->page.free = pages.freesz;
	info->page.boot = pages.bootsz;
	info->page.sz = sizeof(page_t);

	if (info->page.mapsz != -1) {
		for (i = 0, size = 0; i < (pages.freesz + pages.allocsz) / SIZE_PAGE; ++i, ++size) {
			p = pages.pages + i;

			c = pmap_marker(p);
			for (rep = 0; (i + rep + 1) < (pages.freesz + pages.allocsz) / SIZE_PAGE; rep++) {
				if ((c != pmap_marker(pages.pages + i + rep + 1)) || ((pages.pages[i + rep + 1].addr - pages.pages[i + rep].addr) > SIZE_PAGE)) {
					break;
				}
			}

			if (info->page.mapsz > size && info->page.map != NULL) {
				info->page.map[size].count = rep + 1;
				info->page.map[size].marker = c;
				info->page.map[size].addr = p->addr;
			}

			i += rep;
		}

		info->page.mapsz = size;
	}

	proc_lockClear(&pages.lock);
}


void _page_init(pmap_t *pmap, void **bss, void **top)
{
	addr_t addr;
	unsigned int k;
	page_t *page, *p;
	int err;
	void *vaddr;

	proc_lockInit(&pages.lock, &proc_lockAttrDefault, "page");

	/* Prepare memory hash */
	pages.freesz = 0;
	pages.allocsz = 0;
	pages.bootsz = 0;

	for (k = 0; k < SIZE_VM_SIZES; k++) {
		pages.sizes[k] = NULL;
	}

	addr = 0;
	pages.pages = (page_t *)*bss;

	for (page = (page_t *)*bss;;) {

		if ((void *)page + sizeof(page_t) >= (*top)) {
			if (_page_sbrk(pmap, bss, top) < 0) {
				lib_printf("vm: Kernel heap extension error %p %p!\n", page, *top);
				return;
			}
		}

		err = pmap_getPage(page, &addr);
		if (err == -ENOMEM) {
			break;
		}

		if (err == EOK) {

			if ((page->flags & PAGE_FREE) != 0) {
				page->idx = hal_cpuGetFirstBit(SIZE_PAGE);
				LIST_ADD(&pages.sizes[hal_cpuGetFirstBit(SIZE_PAGE)], page);
				pages.freesz += SIZE_PAGE;
			}
			else {
				page->idx = 0;
				pages.allocsz += SIZE_PAGE;
				if (((page->flags >> 1) & 7U) == PAGE_OWNER_BOOT) {
					pages.bootsz += SIZE_PAGE;
				}
			}
			page = page + 1;
		}

		/* Wrap over 0 */
		if (addr < SIZE_PAGE) {
			break;
		}
	}

	(*bss) = page;

	/* Prepare allocation hash */
	_page_initSizes();

	/* Initialize kernel space for user processes */
	for (p = NULL, vaddr = (*top);;) {

		if (_pmap_kernelSpaceExpand(pmap, &vaddr, (*top) + max((pages.freesz + pages.allocsz) / 4, (1 << 23)), p) == 0) {
			break;
		}
		p = _page_alloc(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE);
		if (p == NULL) {
			return;
		}
	}

	/* Show statistics on the console */
	lib_printf("vm: Initializing page allocator (%d+%d)/%dKB, page_t=%d\n", (pages.allocsz - pages.bootsz) / 1024,
			pages.bootsz / 1024, (pages.freesz + pages.allocsz) / 1024, sizeof(page_t));

	_page_showPages();

	/* Create NULL pointer entry */
	_page_map(pmap, NULL, 0, PGHD_USER | ~PGHD_PRESENT);

	return;
}
