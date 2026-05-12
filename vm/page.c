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
#include "include/syspage.h"
#include "lib/lib.h"
#include "proc/lock.h"
#include "proc/proc.h"
#include "include/errno.h"
#include "include/mman.h"
#include "page.h"
#include "hal/types.h"
#include "syspage.h"


#define SIZE_VM_SIZES ((unsigned int)(sizeof(void *) * (size_t)__CHAR_BIT__))


struct _ph_map_t {
	page_t *sizes[SIZE_VM_SIZES];
	page_t *pages; /* pages ordering and their addresses (page_t::addr) stay invariant after _page_init() */

	size_t totalsz; /* stays invariant after _page_init() */
	size_t allocsz;
	size_t bootsz;

	lock_t lock;

	addr_t start;
	addr_t stop;
};

struct {
	ph_map_t **maps;
	size_t mapssz;
	ph_map_t kphmap;
} page_common;


static page_t *_page_alloc(ph_map_t *map, size_t size, vm_flags_t flags, syspage_part_t *part)
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

	if ((part != NULL) && ((part->usedMem + (1UL << start)) > part->availableMem)) {
		return NULL;
	}

	/* Find segment */
	stop = start;

	while ((stop < SIZE_VM_SIZES) && (map->sizes[stop] == NULL)) {
		stop++;
	}
	if (stop == SIZE_VM_SIZES) {
		return NULL;
	}

	lh = map->sizes[stop];

	/* Split segment */
	while (stop > start) {
		LIST_REMOVE(&map->sizes[stop], lh);

		stop--;

		lh->idx--;
		rh = lh + (1UL << lh->idx) / SIZE_PAGE;
		rh->idx = lh->idx;
		LIST_ADD(&map->sizes[stop], lh);
		LIST_ADD(&map->sizes[stop], rh);
	}

	LIST_REMOVE(&map->sizes[stop], lh);

	/* Mark allocated pages */
	for (i = 0; i < (1UL << lh->idx) / SIZE_PAGE; i++) {
		(lh + i)->flags &= ~PAGE_FREE;
		(lh + i)->flags |= flags;
		map->allocsz += SIZE_PAGE;
	}

	if (part != NULL) {
		part->usedMem += (1UL << lh->idx);
	}

	return lh;
}


page_t *vm_pageAlloc(ph_map_t **maps, size_t size, vm_flags_t flags, syspage_part_t *part)
{
	page_t *p;

	while (*maps != NULL) {
		(void)proc_lockSet(&(*maps)->lock);
		p = _page_alloc(*maps, size, flags, part);
		(void)proc_lockClear(&(*maps)->lock);
		if (p != NULL) {
			return p;
		}
		maps++;
	}
	return p;
}


static ph_map_t *page_mapByAddr(addr_t addr)
{
	unsigned int i;
	for (i = 0; i < page_common.mapssz; i++) {
		if ((page_common.maps[i]->start <= addr) && (page_common.maps[i]->stop > addr)) {
			return page_common.maps[i];
		}
	}
	return NULL;
}


void vm_pageFree(page_t *p, syspage_part_t *part)
{
	unsigned int idx, i;
	page_t *lh = p, *rh = p;
	ph_map_t *map;

	if (p == NULL) {
		return;
	}

	map = page_mapByAddr(p->addr);
	if (map == NULL) {
		return;
	}

	(void)proc_lockSet(&map->lock);

	if ((lh->flags & PAGE_FREE) != 0U) {
		hal_cpuDisableInterrupts();
		lib_printf("page: double free (%p)\n", lh);
		hal_cpuEnableInterrupts();
		for (;;) {
		}
	}

	idx = p->idx;

	if (part != NULL) {
		LIB_ASSERT_ALWAYS(part->usedMem >= (1UL << idx), "partition invalid free page.c");
		part->usedMem -= (1UL << idx);
	}

	/* Mark free pages */
	for (i = 0; i < ((u64)1 << idx) / SIZE_PAGE; i++) {
		(p + i)->flags |= PAGE_FREE;
		map->allocsz -= SIZE_PAGE;
	}

	if ((p->addr & (((u64)1 << (idx + 1U)) - 1U)) != 0U) {
		lh = p - ((u64)1 << idx) / SIZE_PAGE;
	}
	else {
		rh = p + ((u64)1 << idx) / SIZE_PAGE;
	}

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 MISRAC2012-RULE_18_3 "lh, rh, pages_info.pages are related" */
	while ((lh >= map->pages) && (rh < (map->pages + map->totalsz / SIZE_PAGE)) &&
			((lh->flags & PAGE_FREE) != 0U) && ((rh->flags & PAGE_FREE) != 0U) && (lh->idx == rh->idx) &&
			((lh->addr + (1UL << lh->idx)) == rh->addr) && (idx < SIZE_VM_SIZES)) {

		if (p == lh) {
			LIST_REMOVE(&map->sizes[idx], rh);
		}
		else {
			LIST_REMOVE(&map->sizes[idx], lh);
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

	LIST_ADD(&map->sizes[idx], p);

	(void)proc_lockClear(&map->lock);
	return;
}


static int page_get_cmp(void *key, void *item)
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


page_t *page_get(addr_t addr)
{
	page_t *p;
	ph_map_t *map = page_mapByAddr(addr);
	if (map == NULL) {
		return NULL;
	}
	size_t np = map->totalsz / SIZE_PAGE;

	addr = addr & ~(SIZE_PAGE - 1U);
	p = lib_bsearch((void *)addr, map->pages, np, sizeof(page_t), page_get_cmp);

	return p;
}


static void _page_initSizes(ph_map_t *map)
{
	unsigned int idx;
	size_t k, i = 0;
	page_t *p;

	/* Remove already discovered pages */
	map->sizes[hal_cpuGetFirstBit(SIZE_PAGE)] = NULL;

	while (i < map->totalsz / SIZE_PAGE) {
		p = &map->pages[i];
		if ((p->flags & PAGE_FREE) == 0U) {
			i++;
			continue;
		}

		idx = hal_cpuGetFirstBit(p->addr);

		if (idx >= SIZE_VM_SIZES) {
			idx = SIZE_VM_SIZES - 1U;
		}

		/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "idx is limited to min(SIZE_VM_SIZES - 1U, bits in p-> addr")*/
		for (k = 0U; (k < (((u64)1 << idx) / SIZE_PAGE) - 1U) && (i + k < ((map->totalsz / SIZE_PAGE) - 1U)); k++) {
			if ((map->pages[i + k + 1U].flags & PAGE_FREE) == 0U) {
				break;
			}
		}

		idx = hal_cpuGetLastBit((k + 1U) * SIZE_PAGE);
		p->idx = (u8)idx;

		LIST_ADD(&map->sizes[idx], p);

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
void _page_showMapPages(ph_map_t *map)
{
	addr_t a = 0;
	page_t *p;
	unsigned int rep, i = 0, k;
	int w;
	char c;
	char buf[TTY_COLS + 1U];

	w = lib_sprintf(buf, "vm: ");
	while (i < map->totalsz / SIZE_PAGE) {
		p = &map->pages[i];

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
		for (rep = 0; ((size_t)i + rep + 1U) < map->totalsz / SIZE_PAGE; rep++) {
			if ((c != pmap_marker(&map->pages[i + rep + 1U])) || (map->pages[i + rep + 1U].addr - map->pages[i + rep].addr > SIZE_PAGE)) {
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

		a = map->pages[i + rep].addr + SIZE_PAGE;
		i += rep + 1U;
	}

	if (w > 4) {
		lib_printf("%s\n", buf);
	}

	return;
}


void _page_showPages(void)
{
	unsigned int map;

	for (map = 0; map < page_common.mapssz; map++) {
		lib_printf("vm: Map %p-%p:\n", (void *)page_common.maps[map]->start, (void *)page_common.maps[map]->stop);
		_page_showMapPages(page_common.maps[map]);
	}
}


static int _page_map(pmap_t *pmap, void *vaddr, addr_t pa, vm_attr_t attr)
{
	page_t *ap = NULL;

	while (pmap_enter(pmap, pa, vaddr, attr, ap) < 0) {
		(void)proc_lockSet(&page_common.kphmap.lock);
		ap = _page_alloc(&page_common.kphmap, SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, NULL);
		(void)proc_lockClear(&page_common.kphmap.lock);
		if (/*vaddr > (void *)VADDR_KERNEL ||*/ ap == NULL) {
			return -ENOMEM;
		}
	}
	return EOK;
}


int page_map(pmap_t *pmap, void *vaddr, addr_t pa, vm_attr_t attr)
{
	// TODO: why was lock here?
	return _page_map(pmap, vaddr, pa, attr);
}


int _page_sbrk(pmap_t *pmap, void **start, void **end)
{
	page_t *np, *ap = NULL;
	np = _page_alloc(&page_common.kphmap, SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP, NULL);
	if (np == NULL) {
		return -ENOMEM;
	}

	while (pmap_enter(pmap, np->addr, (*end), PGHD_READ | PGHD_WRITE | PGHD_PRESENT, ap) < 0) {
		ap = _page_alloc(&page_common.kphmap, SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, NULL);
		if (ap == NULL) {
			return -ENOMEM;
		}
	}

	(*end) += SIZE_PAGE;

	return EOK;
}


void vm_pageGetStats(size_t *freesz)
{
	// TODO: only per map? iterate over all maps? some global values?
	//  *freesz = pages_info.totalsz - pages_info.allocsz;
}


void vm_pageinfo(meminfo_t *info)
{
	// TODO: only per map? iterate over all maps? some global values?
	//  char c;
	//  page_t *p;
	//  unsigned int rep, i = 0;
	//  int size = 0;


	// (void)proc_lockSet(&pages_info.lock);

	// info->page.alloc = (unsigned int)pages_info.allocsz;
	// info->page.free = (unsigned int)(pages_info.totalsz - pages_info.allocsz);
	// info->page.boot = (unsigned int)pages_info.bootsz;
	// info->page.sz = (unsigned int)sizeof(page_t);

	// if (info->page.mapsz != -1) {
	// 	while (i < pages_info.totalsz / SIZE_PAGE) {
	// 		p = pages_info.pages + i;

	// 		c = pmap_marker(p);
	// 		for (rep = 0; ((size_t)i + rep + 1U) < pages_info.totalsz / SIZE_PAGE; rep++) {
	// 			if ((c != pmap_marker(pages_info.pages + i + rep + 1U)) || ((pages_info.pages[i + rep + 1U].addr - pages_info.pages[i + rep].addr) > SIZE_PAGE)) {
	// 				break;
	// 			}
	// 		}

	// 		if (info->page.mapsz > size && info->page.map != NULL) {
	// 			info->page.map[size].count = rep + 1U;
	// 			info->page.map[size].marker = c;
	// 			info->page.map[size].addr = p->addr;
	// 		}

	// 		i += rep + 1U;
	// 		++size;
	// 	}

	// 	info->page.mapsz = size;
	// }

	// (void)proc_lockClear(&pages_info.lock);
}


static size_t _page_initMap(ph_map_t *map, const syspage_map_t *sysMap, pmap_t *pmap, void **bss, void **top)
{
	// TODO: pages could be mapped in provided map instead of kphmap
	addr_t addr;
	unsigned int k;
	page_t *page;
	int err;

	map->start = sysMap->start;
	map->stop = sysMap->end;

	(void)proc_lockInit(&map->lock, &proc_lockAttrDefault, "page");

	/* Prepare memory hash */
	map->totalsz = 0;
	map->allocsz = 0;
	map->bootsz = 0;

	for (k = 0; k < SIZE_VM_SIZES; k++) {
		map->sizes[k] = NULL;
	}

	addr = sysMap->start;
	map->pages = (page_t *)*bss;
	page = (page_t *)*bss;

	for (;;) {
		if ((void *)page + sizeof(page_t) >= (*top)) {
			if (_page_sbrk(pmap, bss, top) < 0) {
				lib_printf("vm: Kernel heap extension error %p %p!\n", page, *top);
				return 0;
			}
		}

		err = pmap_getPage(page, &addr);
		if (err == -ENOMEM) {
			break;
		}

		if (err == EOK) {
			map->totalsz += SIZE_PAGE;
			if ((page->flags & PAGE_FREE) != 0U) {
				page->idx = (u8)hal_cpuGetFirstBit(SIZE_PAGE);
				LIST_ADD(&map->sizes[hal_cpuGetFirstBit(SIZE_PAGE)], page);
			}
			else {
				page->idx = 0;
				map->allocsz += SIZE_PAGE;
				if (((page->flags >> 1U) & 7U) == PAGE_OWNER_BOOT) {
					map->bootsz += SIZE_PAGE;
				}
			}
			page = page + 1;
		}

		/* Skip other masp and don't wrap over 0 */
		if ((addr > sysMap->end) || (addr < SIZE_PAGE)) {
			break;
		}
	}

	(*bss) = page;

	/* Prepare allocation hash */
	_page_initSizes(map);

	return map->totalsz;
}


ph_map_t *vm_getSharedMap(int map)
{
	if ((unsigned int)map >= page_common.mapssz) {
		return NULL;
	}
	return page_common.maps[map];
}


void _page_init(vm_map_t *kmap, void **bss, void **top)
{
	page_t *p;
	void *vaddr;
	const syspage_map_t *sysMap = syspage_mapList();
	size_t totalsz = 0;
	size_t res;
	addr_t addr = pmap_resolve(&kmap->pmap, *bss);
	unsigned int i = 0;

	page_common.kphmap.totalsz = 0;
	/* Initialize kernel map */
	do {
		if ((ptr_t)addr >= (ptr_t)sysMap->start && (ptr_t)addr < (ptr_t)sysMap->end) {
			res = _page_initMap(&page_common.kphmap, sysMap, &kmap->pmap, bss, top);
			if (res == 0) {
				lib_printf("vm: Error during kernel page map initialization!\n");
				return;
			}
			totalsz += res;
			break;
		}
		sysMap = sysMap->next;
	} while (sysMap != syspage_mapList());

	if (page_common.kphmap.totalsz == 0) {
		lib_printf("vm: kernel page map not found in syspage!\n");
		return;
	}

	kmap->phMaps = (ph_map_t **)(*bss);
	res = sizeof(ph_map_t *) * 2;
	if ((void *)kmap->phMaps + res >= (*top)) {
		if (_page_sbrk(&kmap->pmap, bss, top) < 0) {
			lib_printf("vm: Kernel heap extension error %p %p!\n", kmap->phMaps, *top);
			return;
		}
	}
	*bss += res;
	kmap->phMaps[0] = &page_common.kphmap;
	kmap->phMaps[1] = NULL;

	page_common.maps = (ph_map_t **)(*bss);
	res = sizeof(ph_map_t *) * syspage_mapSize();
	if ((void *)page_common.maps + res >= (*top)) {
		if (_page_sbrk(&kmap->pmap, bss, top) < 0) {
			lib_printf("vm: Kernel heap extension error %p %p!\n", page_common.maps, *top);
			return;
		}
	}
	*bss += res;

	do {
		if (sysMap->start == page_common.kphmap.start) {
			page_common.maps[i] = &page_common.kphmap;
			i++;
			sysMap = sysMap->next;
			continue;
		}
		page_common.maps[i] = (ph_map_t *)(*bss);

		if ((void *)page_common.maps[i] + sizeof(ph_map_t) >= (*top)) {
			if (_page_sbrk(&kmap->pmap, bss, top) < 0) {
				lib_printf("vm: Kernel heap extension error %p %p!\n", page_common.maps[i], *top);
				return;
			}
		}
		*bss += sizeof(ph_map_t);
		res = _page_initMap(page_common.maps[i], sysMap, &kmap->pmap, bss, top);
		if (res == 0) {
			lib_printf("vm: Error during page map initialization!\n");
			return;
		}
		totalsz += res;

		i++;
		sysMap = sysMap->next;
	} while (sysMap != syspage_mapList());

	page_common.mapssz = i;

	/* Initialize kernel space for user processes */
	p = NULL;
	vaddr = (*top);

	for (;;) {
		if (_pmap_kernelSpaceExpand(&kmap->pmap, &vaddr, (*top) + max(totalsz / 4U, (1UL << 23)), p) == 0) {
			break;
		}
		p = _page_alloc(&page_common.kphmap, SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, NULL);
		if (p == NULL) {
			return;
		}
	}

	/* Show statistics on the console */
	// lib_printf("vm: Initializing page allocator (%d+%d)/%dKB, page_t=%d\n", (pages_info.allocsz - pages_info.bootsz) / 1024U,
	// 		pages_info.bootsz / 1024U, pages_info.totalsz / 1024U, sizeof(page_t));

	_page_showPages();

	/* Create NULL pointer entry */
	(void)_page_map(&kmap->pmap, NULL, 0, PGHD_USER | ~PGHD_PRESENT);
}
