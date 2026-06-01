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

static struct {
	ph_map_t **maps;
	size_t mapssz;
	lock_t lock;
} page_common;


ph_map_t *vm_getPhysicalMap(int map)
{
	if ((unsigned int)map >= page_common.mapssz) {
		return NULL;
	}
	return page_common.maps[map];
}


static page_t *_page_alloc(ph_map_t *map, size_t size, vm_flags_t flags, partition_t *part)
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

	if (part != NULL) {
		(void)proc_lockSet(&part->lock);
		if ((part->config->availableMem < part->usedMem) ||
				((1UL << start) > part->config->availableMem - part->usedMem)) {
			(void)proc_lockClear(&part->lock);
			return NULL;
		}
	}

	/* Find segment */
	stop = start;

	while ((stop < SIZE_VM_SIZES) && (map->sizes[stop] == NULL)) {
		stop++;
	}
	if (stop == SIZE_VM_SIZES) {
		if (part != NULL) {
			(void)proc_lockClear(&part->lock);
		}
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
		(void)proc_lockClear(&part->lock);
	}

	return lh;
}


static page_t *page_allocAllMaps(size_t size, vm_flags_t flags, partition_t *part)
{
	page_t *p = NULL;
	unsigned int i;

	for (i = 0; i < page_common.mapssz; i++) {
		if (page_common.maps[i] == NULL) {
			continue;
		}
		(void)proc_lockSet(&page_common.maps[i]->lock);
		p = _page_alloc(page_common.maps[i], size, flags, part);
		(void)proc_lockClear(&page_common.maps[i]->lock);
		if (p != NULL) {
			return p;
		}
	}
	return NULL;
}


page_t *vm_pageAlloc(ph_map_t **maps, size_t size, vm_flags_t flags, partition_t *part)
{
	page_t *p = NULL;

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


void vm_pageFree(page_t *p, partition_t *part)
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
		(void)proc_lockSet(&part->lock);
		LIB_ASSERT_ALWAYS(part->usedMem >= (1UL << idx), "partition invalid free page.c");
		part->usedMem -= (1UL << idx);
		(void)proc_lockClear(&part->lock);
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
static void _page_showMapPages(ph_map_t *map)
{
	addr_t a = 0;
	page_t *p;
	unsigned int rep, i = 0, k;
	int w = 0;
	char c;
	char buf[TTY_COLS + 1U];

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

	lib_printf("%s\n", buf);

	return;
}


void _page_showPages(void)
{
	unsigned int map;

	for (map = 0; map < page_common.mapssz; map++) {
		lib_printf("vm: Map 0x%x:0x%x ", (void *)page_common.maps[map]->start, (void *)page_common.maps[map]->stop);
		_page_showMapPages(page_common.maps[map]);
	}
}


static int _page_map(pmap_t *pmap, void *vaddr, addr_t pa, vm_attr_t attr)
{
	page_t *ap = NULL;

	while (pmap_enter(pmap, pa, vaddr, attr, ap) < 0) {
		ap = page_allocAllMaps(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, NULL);
		if (/*vaddr > (void *)VADDR_KERNEL ||*/ ap == NULL) {
			return -ENOMEM;
		}
	}
	return EOK;
}


int page_map(pmap_t *pmap, void *vaddr, addr_t pa, vm_attr_t attr)
{
	int err;

	(void)proc_lockSet(&page_common.lock);
	err = _page_map(pmap, vaddr, pa, attr);
	(void)proc_lockClear(&page_common.lock);

	return err;
}


int _page_sbrk(pmap_t *pmap, void **start, void **end)
{
	page_t *np, *ap = NULL;
	np = page_allocAllMaps(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP, NULL);
	if (np == NULL) {
		return -ENOMEM;
	}

	while (pmap_enter(pmap, np->addr, (*end), PGHD_READ | PGHD_WRITE | PGHD_PRESENT, ap) < 0) {
		ap = page_allocAllMaps(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, NULL);
		if (ap == NULL) {
			return -ENOMEM;
		}
	}

	(*end) += SIZE_PAGE;

	return EOK;
}


void _vm_pageGetStats(size_t *freesz)
{
	*freesz = 0;
	for (size_t i = 0; i < page_common.mapssz; i++) {
		*freesz += page_common.maps[i]->totalsz - page_common.maps[i]->allocsz;
	}
}


void vm_pageinfo(meminfo_t *info)
{
	char c;
	page_t *p;
	int idx;
	unsigned int rep, i = 0;
	int size = 0;

	info->page.sz = (unsigned int)sizeof(page_t);
	info->page.mapsz = (int)page_common.mapssz;
	info->page.alloc = 0;
	info->page.total = 0;
	info->page.boot = 0;
	for (i = 0; i < page_common.mapssz; i++) {
		(void)proc_lockSet(&page_common.maps[i]->lock);
		info->page.alloc += (unsigned int)page_common.maps[i]->allocsz;
		info->page.total += (unsigned int)page_common.maps[i]->totalsz;
		info->page.boot += (unsigned int)page_common.maps[i]->bootsz;
		(void)proc_lockClear(&page_common.maps[i]->lock);
	}

	idx = info->page.mapidx;
	if ((idx == -1) || (idx >= (int)page_common.mapssz)) {
		return;
	}

	(void)proc_lockSet(&page_common.maps[idx]->lock);

	info->page.map.alloc = (unsigned int)page_common.maps[idx]->allocsz;
	info->page.map.free = (unsigned int)(page_common.maps[idx]->totalsz - page_common.maps[idx]->allocsz);
	info->page.map.boot = (unsigned int)page_common.maps[idx]->bootsz;

	if (info->page.map.mapsz != -1) {
		i = 0;
		while (i < page_common.maps[idx]->totalsz / SIZE_PAGE) {
			p = page_common.maps[idx]->pages + i;

			c = pmap_marker(p);
			for (rep = 0; ((size_t)i + rep + 1U) < page_common.maps[idx]->totalsz / SIZE_PAGE; rep++) {
				if ((c != pmap_marker(page_common.maps[idx]->pages + i + rep + 1U)) || ((page_common.maps[idx]->pages[i + rep + 1U].addr - page_common.maps[idx]->pages[i + rep].addr) > SIZE_PAGE)) {
					break;
				}
			}

			if (info->page.map.mapsz > size && info->page.map.map != NULL) {
				info->page.map.map[size].count = rep + 1U;
				info->page.map.map[size].marker = c;
				info->page.map.map[size].addr = p->addr;
			}

			i += rep + 1U;
			++size;
		}

		info->page.map.mapsz = size;
	}

	(void)proc_lockClear(&page_common.maps[idx]->lock);
}


static int _page_initMap(ph_map_t *map, const syspage_map_t *sysMap, pmap_t *pmap, void **bss, void **top)
{
	addr_t addr;
	unsigned int k;
	page_t *page;
	int err;

	map->start = sysMap->start;
	map->stop = sysMap->end;

	(void)proc_lockInit(&map->lock, &proc_lockAttrDefault, "page.map");

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
				return -ENOMEM;
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

		/* Skip other maps and don't wrap over 0 */
		if ((addr >= sysMap->end) || (addr < SIZE_PAGE)) {
			break;
		}
	}

	(*bss) = page;

	/* Prepare allocation hash */
	_page_initSizes(map);

	return 0;
}


void _page_init(vm_map_t *kmap, void **bss, void **top)
{
	meminfo_t info;
	page_t *p;
	void *vaddr;
	const syspage_map_t *sysMap = syspage_mapList();
	size_t size;
	unsigned int i = 0;
	(void)proc_lockInit(&page_common.lock, &proc_lockAttrDefault, "page.common");

	page_common.maps = (ph_map_t **)(*bss);
	page_common.mapssz = syspage_mapSize();
	size = sizeof(ph_map_t *) * (page_common.mapssz + 1U);
	if ((ptr_t)page_common.maps + size >= (ptr_t)(*top)) {
		if (_page_sbrk(&kmap->pmap, bss, top) < 0) {
			lib_printf("vm: Kernel heap extension error %p %p!\n", page_common.maps, *top);
			return;
		}
	}
	*bss += size;
	hal_memset(page_common.maps, 0, size);

	/* Initialize maps */
	do {
		page_common.maps[i] = (ph_map_t *)(*bss);
		if ((ptr_t)page_common.maps[i] + sizeof(ph_map_t) >= (ptr_t)(*top)) {
			if (_page_sbrk(&kmap->pmap, bss, top) < 0) {
				lib_printf("vm: Kernel heap extension error %p %p!\n", page_common.maps[i], *top);
				return;
			}
		}
		*bss += sizeof(ph_map_t);
		if (_page_initMap(page_common.maps[i], sysMap, &kmap->pmap, bss, top) != 0) {
			lib_printf("vm: Error during physical map initialization!\n");
			return;
		}

		i++;
		sysMap = sysMap->next;
	} while (sysMap != syspage_mapList());

	kmap->phMaps = page_common.maps;

	info.page.mapidx = -1;
	vm_pageinfo(&info);

	/* Initialize kernel space for user processes */
	p = NULL;
	vaddr = (*top);

	for (;;) {
		if (_pmap_kernelSpaceExpand(&kmap->pmap, &vaddr, (*top) + max(info.page.total / 4U, (1UL << 23)), p) == 0) {
			break;
		}
		p = page_allocAllMaps(SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, NULL);
		if (p == NULL) {
			return;
		}
	}

	/* Show statistics on the console */
	lib_printf("vm: Initializing page allocator (%d+%d)/%dKB, page_t=%d\n", (info.page.alloc - info.page.boot) / 1024U,
			info.page.boot / 1024U, (info.page.total) / 1024U, sizeof(page_t));

	_page_showPages();

	/* Create NULL pointer entry */
	(void)page_map(&kmap->pmap, NULL, 0, PGHD_USER | ~PGHD_PRESENT);
}
