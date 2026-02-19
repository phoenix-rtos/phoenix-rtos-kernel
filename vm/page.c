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
#include "hal/types.h"


#define SIZE_VM_SIZES ((unsigned int)(sizeof(void *) * (size_t)__CHAR_BIT__))


static struct {
	page_t *sizes[SIZE_VM_SIZES];
	page_t *pages;

	size_t allocsz;
	size_t bootsz;
	size_t freesz;

	lock_t lock;
} pages_info;


static page_t *_page_alloc(size_t size, vm_flags_t flags, partition_t *part)
{
	unsigned int start, stop, i;
	page_t *lh, *rh;

	/* Establish first index */
	size = (size < SIZE_PAGE) ? SIZE_PAGE : size;

	start = hal_cpuGetLastBit(size);
	/* parasoft-suppress-next-line MISRAC2012-RULE_14_3 "conditional compilation" */
	if (hal_cpuGetFirstBit(size) < start) {
		start++;
	}

	if ((part != NULL) && ((part->usedMem + (1U << start)) > part->config->availableMem)) {
		return NULL;
	}

	/* Find segment */
	stop = start;

	while ((stop < SIZE_VM_SIZES) && (pages_info.sizes[stop] == NULL)) {
		stop++;
	}
	if (stop == SIZE_VM_SIZES) {
		return NULL;
	}

	lh = pages_info.sizes[stop];

	/* Split segment */
	while (stop > start) {
		LIST_REMOVE(&pages_info.sizes[stop], lh);

		stop--;

		lh->idx--;
		rh = lh + (1UL << lh->idx) / SIZE_PAGE;
		rh->idx = lh->idx;
		LIST_ADD(&pages_info.sizes[stop], lh);
		LIST_ADD(&pages_info.sizes[stop], rh);
	}

	LIST_REMOVE(&pages_info.sizes[stop], lh);

	/* Mark allocated pages */
	for (i = 0; i < (1UL << lh->idx) / SIZE_PAGE; i++) {
		(lh + i)->flags &= ~PAGE_FREE;
		(lh + i)->flags |= flags;
		pages_info.freesz -= SIZE_PAGE;
		pages_info.allocsz += SIZE_PAGE;
	}

	if (part != NULL) {
		part->usedMem += (1U << lh->idx);
	}

	return lh;
}


page_t *vm_pageAlloc(size_t size, vm_flags_t flags, partition_t *part)
{
	page_t *p;

	(void)proc_lockSet(&pages_info.lock);
	p = _page_alloc(size, flags, part);
	(void)proc_lockClear(&pages_info.lock);
	return p;
}


void vm_pageFree(page_t *p, partition_t *part)
{
	unsigned int idx, i;
	page_t *lh = p, *rh = p;

	if (p == NULL) {
		return;
	}

	(void)proc_lockSet(&pages_info.lock);

	if ((lh->flags & PAGE_FREE) != 0U) {
		hal_cpuDisableInterrupts();
		lib_printf("page: double free (%p)\n", lh);
		hal_cpuEnableInterrupts();
		for (;;) {
		}
	}

	idx = p->idx;

	if (part != NULL) {
		LIB_ASSERT_ALWAYS(part->usedMem >= (1U << idx), "partition invalid free page.c");
		part->usedMem -= (1U << idx);
	}

	/* Mark free pages */
	for (i = 0; i < ((u64)1 << idx) / SIZE_PAGE; i++) {
		(p + i)->flags |= PAGE_FREE;
		pages_info.freesz += SIZE_PAGE;
		pages_info.allocsz -= SIZE_PAGE;
	}

	if ((p->addr & (((u64)1 << (idx + 1U)) - 1U)) != 0U) {
		lh = p - ((u64)1 << idx) / SIZE_PAGE;
	}
	else {
		rh = p + ((u64)1 << idx) / SIZE_PAGE;
	}

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 MISRAC2012-RULE_18_3 "lh, rh, pages_info.pages are related" */
	while ((lh >= pages_info.pages) && (rh < (pages_info.pages + (pages_info.allocsz + pages_info.freesz) / SIZE_PAGE)) &&
			((lh->flags & PAGE_FREE) != 0U) && ((rh->flags & PAGE_FREE) != 0U) && (lh->idx == rh->idx) &&
			((lh->addr + (1UL << lh->idx)) == rh->addr) && (idx < SIZE_VM_SIZES)) {

		if (p == lh) {
			LIST_REMOVE(&pages_info.sizes[idx], rh);
		}
		else {
			LIST_REMOVE(&pages_info.sizes[idx], lh);
		}

		rh->idx = (u8)hal_cpuGetFirstBit(SIZE_PAGE);
		lh->idx++;
		idx++;

		p = lh;

		if ((p->addr & (((u64)1 << (idx + 1U)) - 1U)) != 0U) {
			lh = p - ((u64)1 << idx) / SIZE_PAGE;
		}
		else {
			rh = p + ((u64)1 << idx) / SIZE_PAGE;
		}
	}

	LIST_ADD(&pages_info.sizes[idx], p);

	(void)proc_lockClear(&pages_info.lock);
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
	size_t np = (pages_info.freesz + pages_info.allocsz) / SIZE_PAGE;

	addr = addr & ~(SIZE_PAGE - 1U);
	p = lib_bsearch((void *)addr, pages_info.pages, np, sizeof(page_t), _page_get_cmp);

	return p;
}


static void _page_initSizes(void)
{
	unsigned int idx;
	size_t k, i = 0;
	page_t *p;

	/* Remove already discovered pages */
	pages_info.sizes[hal_cpuGetFirstBit(SIZE_PAGE)] = NULL;

	while (i < (pages_info.allocsz + pages_info.freesz) / SIZE_PAGE) {
		p = &pages_info.pages[i];
		if ((p->flags & PAGE_FREE) == 0U) {
			i++;
			continue;
		}

		idx = hal_cpuGetFirstBit(p->addr);

		if (idx >= SIZE_VM_SIZES) {
			idx = SIZE_VM_SIZES - 1U;
		}

		/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "idx is limited to min(SIZE_VM_SIZES - 1U, bits in p-> addr")*/
		for (k = 0U; (k < (((u64)1 << idx) / SIZE_PAGE) - 1U) && (i + k < (((pages_info.allocsz + pages_info.freesz) / SIZE_PAGE) - 1U)); k++) {
			if ((pages_info.pages[i + k + 1U].flags & PAGE_FREE) == 0U) {
				break;
			}
		}

		idx = hal_cpuGetLastBit((k + 1U) * SIZE_PAGE);
		p->idx = (u8)idx;

		LIST_ADD(&pages_info.sizes[idx], p);

		i += (size_t)(((u64)1 << idx) / SIZE_PAGE);
	}
	return;
}


static unsigned int page_digits(unsigned int n, unsigned int base)
{
	unsigned int d = 0;

	do {
		n /= base;
		d++;
	} while (n != 0U);

	return d;
}


#define TTY_COLS 80U
void _page_showPages(void)
{
	addr_t a = 0;
	page_t *p;
	unsigned int rep, i = 0, k;
	int w;
	char c;
	char buf[TTY_COLS + 1U];

	w = lib_sprintf(buf, "vm: ");
	while (i < (pages_info.freesz + pages_info.allocsz) / SIZE_PAGE) {
		p = &pages_info.pages[i];

		/* Print markers in case of memory gap */
		if (p->addr > a) {
			rep = (unsigned int)(p->addr - a) / (unsigned int)SIZE_PAGE;
			if (rep >= 4U) {
				k = page_digits(rep, 10) + 3U;
				if ((unsigned int)w + k > TTY_COLS) {
					lib_printf("%s\n", buf);
					w = lib_sprintf(buf, "vm: ");
				}
				w += lib_sprintf(buf + w, "[%dx]", rep);
			}
			else {
				for (k = 0; k < rep; k++) {
					if ((unsigned int)w + 1U > TTY_COLS) {
						lib_printf("%s\n", buf);
						w = lib_sprintf(buf, "vm: ");
					}
					w += lib_sprintf(buf + w, "%c", 'x');
				}
			}
		}

		/* Print markers with repetitions */
		c = pmap_marker(p);
		for (rep = 0; ((size_t)i + rep + 1U) < (pages_info.freesz + pages_info.allocsz) / SIZE_PAGE; rep++) {
			if ((c != pmap_marker(&pages_info.pages[i + rep + 1U])) || (pages_info.pages[i + rep + 1U].addr - pages_info.pages[i + rep].addr > SIZE_PAGE)) {
				break;
			}
		}

		if (rep >= 4U) {
			k = page_digits(rep + 1U, 10) + 3U;
			if ((unsigned int)w + k > TTY_COLS) {
				lib_printf("%s\n", buf);
				w = lib_sprintf(buf, "vm: ");
			}
			w += lib_sprintf(buf + w, "[%d%c]", rep + 1U, c);
		}
		else {
			for (k = 0; k <= rep; k++) {
				if ((unsigned int)w + 1U > TTY_COLS) {
					lib_printf("%s\n", buf);
					w = lib_sprintf(buf, "vm: ");
				}
				w += lib_sprintf(buf + w, "%c", pmap_marker(p));
			}
		}

		a = pages_info.pages[i + rep].addr + SIZE_PAGE;
		i += rep + 1U;
	}

	if (w > 4) {
		lib_printf("%s\n", buf);
	}

	return;
}


static int _page_map(pmap_t *pmap, void *vaddr, addr_t pa, vm_attr_t attr)
{
	page_t *ap = NULL;

	while (pmap_enter(pmap, pa, vaddr, attr, ap) < 0) {
		ap = _page_alloc(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, NULL);
		if (/*vaddr > (void *)VADDR_KERNEL ||*/ ap == NULL) {
			return -ENOMEM;
		}
	}
	return EOK;
}


int page_map(pmap_t *pmap, void *vaddr, addr_t pa, vm_attr_t attr)
{
	int err;

	(void)proc_lockSet(&pages_info.lock);
	err = _page_map(pmap, vaddr, pa, attr);
	(void)proc_lockClear(&pages_info.lock);

	return err;
}


int _page_sbrk(pmap_t *pmap, void **start, void **end)
{
	page_t *np, *ap = NULL;
	np = _page_alloc(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP, NULL);
	if (np == NULL) {
		return -ENOMEM;
	}

	while (pmap_enter(pmap, np->addr, (*end), PGHD_READ | PGHD_WRITE | PGHD_PRESENT, ap) < 0) {
		ap = _page_alloc(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, NULL);
		if (ap == NULL) {
			return -ENOMEM;
		}
	}

	(*end) += SIZE_PAGE;

	return EOK;
}


void vm_pageGetStats(size_t *freesz)
{
	*freesz = pages_info.freesz;
}


void vm_pageinfo(meminfo_t *info)
{
	char c;
	page_t *p;
	unsigned int rep, i = 0;
	int size = 0;

	(void)proc_lockSet(&pages_info.lock);

	info->page.alloc = (unsigned int)pages_info.allocsz;
	info->page.free = (unsigned int)pages_info.freesz;
	info->page.boot = (unsigned int)pages_info.bootsz;
	info->page.sz = (unsigned int)sizeof(page_t);

	if (info->page.mapsz != -1) {
		while (i < (pages_info.freesz + pages_info.allocsz) / SIZE_PAGE) {
			p = pages_info.pages + i;

			c = pmap_marker(p);
			for (rep = 0; ((size_t)i + rep + 1U) < (pages_info.freesz + pages_info.allocsz) / SIZE_PAGE; rep++) {
				if ((c != pmap_marker(pages_info.pages + i + rep + 1U)) || ((pages_info.pages[i + rep + 1U].addr - pages_info.pages[i + rep].addr) > SIZE_PAGE)) {
					break;
				}
			}

			if (info->page.mapsz > size && info->page.map != NULL) {
				info->page.map[size].count = rep + 1U;
				info->page.map[size].marker = c;
				info->page.map[size].addr = p->addr;
			}

			i += rep + 1U;
			++size;
		}

		info->page.mapsz = size;
	}

	(void)proc_lockClear(&pages_info.lock);
}


void _page_init(pmap_t *pmap, void **bss, void **top)
{
	addr_t addr;
	unsigned int k;
	page_t *page, *p;
	int err;
	void *vaddr;

	(void)proc_lockInit(&pages_info.lock, &proc_lockAttrDefault, "page");

	/* Prepare memory hash */
	pages_info.freesz = 0;
	pages_info.allocsz = 0;
	pages_info.bootsz = 0;

	for (k = 0; k < SIZE_VM_SIZES; k++) {
		pages_info.sizes[k] = NULL;
	}

	addr = 0;
	pages_info.pages = (page_t *)*bss;
	page = (page_t *)*bss;

	for (;;) {
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

			if ((page->flags & PAGE_FREE) != 0U) {
				page->idx = (u8)hal_cpuGetFirstBit(SIZE_PAGE);
				LIST_ADD(&pages_info.sizes[hal_cpuGetFirstBit(SIZE_PAGE)], page);
				pages_info.freesz += SIZE_PAGE;
			}
			else {
				page->idx = 0;
				pages_info.allocsz += SIZE_PAGE;
				if (((page->flags >> 1U) & 7U) == PAGE_OWNER_BOOT) {
					pages_info.bootsz += SIZE_PAGE;
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
	p = NULL;
	vaddr = (*top);

	for (;;) {
		if (_pmap_kernelSpaceExpand(pmap, &vaddr, (*top) + max((pages_info.freesz + pages_info.allocsz) / 4U, (1UL << 23)), p) == 0) {
			break;
		}
		p = _page_alloc(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, NULL);
		if (p == NULL) {
			return;
		}
	}

	/* Show statistics on the console */
	lib_printf("vm: Initializing page allocator (%d+%d)/%dKB, page_t=%d\n", (pages_info.allocsz - pages_info.bootsz) / 1024U,
			pages_info.bootsz / 1024U, (pages_info.freesz + pages_info.allocsz) / 1024U, sizeof(page_t));

	_page_showPages();

	/* Create NULL pointer entry */
	(void)_page_map(pmap, NULL, 0, PGHD_USER | ~PGHD_PRESENT);

	return;
}
