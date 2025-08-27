/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - page allocator
 *
 * Copyright 2012, 2016, 2017 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "proc/proc.h"
#include "lib/lib.h"
#include "include/errno.h"
#include "page.h"
#include "map.h"
#include "syspage.h"


extern unsigned int __bss_start;


static struct {
	size_t allocsz;
	size_t bootsz;
	size_t freesz;

	page_t *freeq;
	size_t freeqsz;

	lock_t lock;
} pages;


static page_t *_page_alloc(size_t size, u8 flags)
{
	page_t *lh = pages.freeq;

	if (lh == NULL) {
		return NULL;
	}

	pages.freeq = lh->next;

	lh->next = NULL;
	lh->idx = (u8)hal_cpuGetLastBit(size);

	if (hal_cpuGetFirstBit(size) < lh->idx) {
		lh->idx++;
	}

	pages.freesz -= (1UL << lh->idx);
	pages.allocsz += (1UL << lh->idx);

	return lh;
}


page_t *vm_pageAlloc(size_t size, u8 flags)
{
	page_t *p;

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 70, 72*/
	(void)proc_lockSet(&pages.lock);
	p = _page_alloc(size, flags);
	(void)proc_lockClear(&pages.lock);

	return p;
}


void vm_pageFree(page_t *lh)
{
	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 80, 89*/
	(void)proc_lockSet(&pages.lock);

	lh->next = pages.freeq;
	pages.freeq = lh;

	pages.freesz += (1UL << lh->idx);
	pages.allocsz -= (1UL << lh->idx);

	(void)proc_lockClear(&pages.lock);
	return;
}


void _page_showPages(void)
{
	return;
}


int page_map(pmap_t *pmap, void *vaddr, addr_t pa, unsigned int attr)
{
	page_t *ap;

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 105, 108, 111, 113*/
	(void)proc_lockSet(&pages.lock);
	if (pmap_enter(pmap, pa, vaddr, (int)attr, NULL) < 0) {
		if ((ap = _page_alloc(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE)) == NULL) {
			(void)proc_lockClear(&pages.lock);
			return -ENOMEM;
		}
		(void)pmap_enter(pmap, pa, vaddr, (int)attr, ap);
	}
	(void)proc_lockClear(&pages.lock);

	return EOK;
}


page_t *_page_get(addr_t addr)
{
	return NULL;
}


int _page_sbrk(pmap_t *pmap, void **start, void **end)
{
	page_t *np;

	if ((np = _page_alloc(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP)) == NULL) {
		return -ENOMEM;
	}

	if (page_map(pmap, (*end), PGHD_PRESENT | PGHD_WRITE, (addr_t)np) < 0) {
		return -ENOMEM;
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
	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 152, 160*/
	(void)proc_lockSet(&pages.lock);

	info->page.alloc = pages.allocsz;
	info->page.free = pages.freesz;
	info->page.boot = pages.bootsz;
	info->page.sz = sizeof(page_t);
	info->page.mapsz = -1;

	(void)proc_lockClear(&pages.lock);
}


void _page_init(pmap_t *pmap, void **bss, void **top)
{
	page_t *p;
	const syspage_map_t *map;
	unsigned int i;

	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)proc_lockInit(&pages.lock, &proc_lockAttrDefault, "page.nommu");

	/* TODO: handle error */
	if ((map = syspage_mapAddrResolve((addr_t)&__bss_start)) == NULL) {
		return;
	}

	pages.freesz = map->end - (unsigned int)(*bss);
	pages.bootsz = 0;

	pages.freeq = (*bss);
	pages.freeqsz = pages.freesz / SIZE_PAGE;

	(*bss) += pages.freeqsz * sizeof(page_t);
	(*top) = max((*top), (*bss));

	/* Prepare memory hash */
	pages.allocsz = (unsigned int)(*bss) - (unsigned int)&__bss_start;
	pages.freesz -= pages.freeqsz * sizeof(page_t);

	/* Show statistics one the console */
	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)lib_printf("vm: Initializing page allocator %d/%d KB, page_t=%d\n", (pages.allocsz - pages.bootsz) / 1024U,
			(pages.freesz + pages.allocsz) / 1024U, sizeof(page_t));

	/* Prepare allocation queue */
	for (p = pages.freeq, i = 0; i < pages.freeqsz; i++) {
		p->next = p + 1;
		p = p->next;
	}
	(pages.freeq + pages.freeqsz - 1)->next = NULL;

	return;
}
