/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - page allocator
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Klimek
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "lib/lib.h"
#include "lib/rb.h"
#include "proc/proc.h"
#include "include/errno.h"
#include "phmap.h"


#ifndef PH_ALIGN /* should be divisor of page size */
#define PH_ALIGN SIZE_PAGE
#endif


typedef struct _ph_map_t {
	addr_t start;
	addr_t stop;
	rbtree_t tree;
	lock_t lock;
} ph_map_t;


typedef struct _ph_entry_t {
	union {
		rbnode_t linkage;
		struct _ph_entry_t *next;
	};
	size_t lmaxgap;
	size_t rmaxgap;
	size_t allocsz;
	ph_map_t *map;

	addr_t addr;
	size_t size;

	page_flags_t flags;
} ph_entry_t;


static struct {
	lock_t lock;

	size_t ntotal, nfree;
	ph_entry_t *free;

	ph_map_t **maps;
	size_t mapssz;

	size_t bootsz;
} phmap_common;


// TODO: vmlib with shared functions (with map.c) to reduce kernel size?

static int _phmap_cmp(rbnode_t *n1, rbnode_t *n2)
{
	ph_entry_t *e1 = lib_treeof(ph_entry_t, linkage, n1);
	ph_entry_t *e2 = lib_treeof(ph_entry_t, linkage, n2);

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
	if ((ptr_t)e2->addr + e2->size <= (ptr_t)e1->addr) {
		return 1;
	}

	if ((ptr_t)e1->addr + e1->size <= (ptr_t)e2->addr) {
		return -1;
	}

	return 0;
}


static void _phmap_augment(rbnode_t *node)
{
	rbnode_t *it;
	ph_entry_t *n = lib_treeof(ph_entry_t, linkage, node);
	ph_entry_t *p = n;
	ph_entry_t *l, *r;

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
	n->allocsz = n->size;

	if (node->left == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(ph_entry_t, linkage, it->parent);
			if (it->parent->right == it) {
				break;
			}
		}

		/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
		n->lmaxgap = ((ptr_t)n->addr <= (ptr_t)p->addr) ? ((size_t)n->addr - (size_t)n->map->start) : ((size_t)n->addr - (size_t)p->addr) - p->size;
	}
	else {
		l = lib_treeof(ph_entry_t, linkage, node->left);
		/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
		n->lmaxgap = max(l->lmaxgap, l->rmaxgap);
		n->allocsz += l->allocsz;
	}

	if (node->right == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(ph_entry_t, linkage, it->parent);
			if (it->parent->left == it) {
				break;
			}
		}

		n->rmaxgap = ((ptr_t)n->addr >= (ptr_t)p->addr) ? ((size_t)n->map->stop - (size_t)n->addr) - n->size : ((size_t)p->addr - (size_t)n->addr) - n->size;
	}
	else {
		r = lib_treeof(ph_entry_t, linkage, node->right);
		n->rmaxgap = max(r->lmaxgap, r->rmaxgap);
		n->allocsz += r->allocsz;
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(ph_entry_t, linkage, it);
		p = lib_treeof(ph_entry_t, linkage, it->parent);

		if (it->parent->left == it) {
			p->lmaxgap = max(n->lmaxgap, n->rmaxgap);
		}
		else {
			p->rmaxgap = max(n->lmaxgap, n->rmaxgap);
		}

		l = lib_treeof(ph_entry_t, linkage, p->linkage.left);
		r = lib_treeof(ph_entry_t, linkage, p->linkage.right);

		p->allocsz = p->size;

		if (l != NULL) {
			p->allocsz += l->allocsz;
		}
		if (r != NULL) {
			p->allocsz += r->allocsz;
		}
	}
}


static int _phmap_find(ph_map_t *map, addr_t req_addr, size_t size, ph_entry_t **prev, ph_entry_t **next, addr_t *res)
{
	ph_entry_t *e = lib_treeof(ph_entry_t, linkage, map->tree.root);

	*prev = NULL;
	*next = NULL;

	if (req_addr < map->start) {
		req_addr = map->start;
	}

	req_addr = req_addr & ~(PH_ALIGN - 1U);

	if ((map->stop < size) || ((map->stop - size + 1U) < req_addr)) {
		return -ENOMEM;
	}


	while (e != NULL) {

		if ((size <= e->lmaxgap) && ((req_addr + size) <= e->addr)) {
			*next = e;

			if (e->linkage.left == NULL) {
				*res = max(req_addr, e->addr - e->lmaxgap);
				return EOK;
			}
			e = lib_treeof(ph_entry_t, linkage, e->linkage.left);
			continue;
		}

		if ((size <= e->rmaxgap) /*&& (req_addr + size) <= (e->addr + e->size + e->rmaxgap)*/) {
			*prev = e;

			if (e->linkage.right == NULL) {
				*res = max(req_addr, e->addr + e->size);
				return EOK;
			}
			e = lib_treeof(ph_entry_t, linkage, e->linkage.right);
			continue;
		}

		for (;;) {
			if (e->linkage.parent == NULL) {
				return -ENOMEM;
			}

			if ((e == lib_treeof(ph_entry_t, linkage, e->linkage.parent->left)) && ((lib_treeof(ph_entry_t, linkage, e->linkage.parent)->rmaxgap >= size))) {
				break;
			}
			e = lib_treeof(ph_entry_t, linkage, e->linkage.parent);
		}
		e = lib_treeof(ph_entry_t, linkage, e->linkage.parent);

		for (*next = e; (*next)->linkage.parent != NULL; *next = lib_treeof(ph_entry_t, linkage, (*next)->linkage.parent)) {
			if ((*next) == lib_treeof(ph_entry_t, linkage, (*next)->linkage.parent->left)) {
				break;
			}
		}

		*next = lib_treeof(ph_entry_t, linkage, (*next)->linkage.parent);

		*prev = e;
		if (e->linkage.right == NULL) {
			*res = e->addr + e->size;
			return EOK;
		}

		e = lib_treeof(ph_entry_t, linkage, e->linkage.right);
	}

	*res = req_addr;
	return EOK;
}


static void _phmap_entryfree(ph_map_t *map, ph_entry_t *entry)
{
	lib_rbRemove(&map->tree, &entry->linkage);

	(void)proc_lockSet(&phmap_common.lock);
	entry->next = phmap_common.free;
	entry->addr = PHADDR_INVALID;
	phmap_common.free = entry;
	phmap_common.nfree++;
	(void)proc_lockClear(&phmap_common.lock);
}


static int _phmap_alloc(ph_map_t *map, addr_t *addr, size_t size, page_flags_t page_flags, vm_flags_t vm_flags, ph_entry_t **newEntry);


static ph_entry_t *phmap_entryalloc(void)
{
	ph_entry_t *res;

	(void)proc_lockSet(&phmap_common.lock);

	if (phmap_common.nfree == 0U) {
		(void)proc_lockClear(&phmap_common.lock);
		return NULL;
	}

	LIB_ASSERT_ALWAYS(phmap_common.free != NULL, "phmap: nfree > 0 but free list is empty");
	LIB_ASSERT_ALWAYS(phmap_common.free->addr == PHADDR_INVALID, "not invalid addr in free phmap entry %p", phmap_common.free);

	/* TODO: dynamic allocation of more entries */
	// if(phmap_common.nfree == 1) {
	// change it to guarantee enough to make sure mmap won't fail + 1 potentially created here
	// 	addr = 0U;
	// TODO: Kernel maps only...
	// 	for (map in maps){
	// 		err = _phmap_alloc(map, addr, SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP, 0, &phmap_common.free);
	// 	}
	// 	if (err...) {

	// 	}
	// 	phmap_common.free = vm_mmap(kmap, NULL, addr, s, PROT_READ | PROT_WRITE, kernel, VM_OFFS_MAX, MAP_NONE);
	// 	for (i = 0; i < SIZE_PAGE; i += sizeof(ph_entry_t)) {
	// 		res = (ph_entry_t *)(phmap_common.free + i);
	// 		res->next = phmap_common.free;
	// 		phmap_common.free = res;
	// 		phmap_common.nfree++;
	// 	}
	// }

	res = phmap_common.free;
	phmap_common.free = res->next;
	res->next = NULL;
	phmap_common.nfree--;
	(void)proc_lockClear(&phmap_common.lock);

	return res;
}


static int _phmap_alloc(ph_map_t *map, addr_t *addr, size_t size, page_flags_t page_flags, vm_flags_t vm_flags, ph_entry_t **newEntry)  // TODO: style of naming in whole file...
{
	ph_entry_t *prev, *next, *e;
	unsigned int lmerge, rmerge;
	addr_t res;
	int err;

	err = _phmap_find(map, *addr, size, &prev, &next, &res);
	if (err != EOK) {
		return err;
	}
	if (((vm_flags & MAP_FIXED) != 0U) && (res != *addr)) {
		return -ENOMEM;
	}
	*addr = res;

	LIB_ASSERT_ALWAYS(next == NULL || *addr + size <= next->addr, "phmap: _phmap_alloc found invalid next entry");
	LIB_ASSERT_ALWAYS(prev == NULL || prev->addr + prev->size <= *addr, "phmap: _phmap_alloc found invalid prev entry");

	ph_entry_t ee, *eee;
	ee.addr = *addr;
	ee.size = 1;
	eee = lib_treeof(ph_entry_t, linkage, lib_rbFind(&map->tree, &ee.linkage));
	LIB_ASSERT_ALWAYS(eee == NULL, "phmap: _phmap_alloc found overlapping entry %p", eee);

	rmerge = (next != NULL && *addr + size == next->addr && next->flags == page_flags) ? 1U : 0U;
	lmerge = (prev != NULL && *addr == prev->addr + prev->size && prev->flags == page_flags) ? 1U : 0U;

	if (rmerge != 0U && lmerge != 0U) {
		e = prev;
		e->size += size + next->size;

		_phmap_augment(&e->linkage);
		_phmap_entryfree(map, next);
	}
	else if (rmerge != 0U) {
		e = next;
		e->addr = *addr;
		e->size += size;

		if (prev != NULL) {
			_phmap_augment(&prev->linkage);
		}

		_phmap_augment(&e->linkage);
	}
	else if (lmerge != 0U) {
		e = prev;
		e->size += size;

		if (next != NULL) {
			_phmap_augment(&next->linkage);
		}

		_phmap_augment(&e->linkage);
	}
	else {
		if (newEntry != NULL) {
			e = *newEntry;
			*newEntry = e->next;
		}
		else {
			e = phmap_entryalloc();
			if (e == NULL) {
				return -ENOMEM;
			}
		}

		e->addr = *addr;
		e->size = size;
		e->flags = page_flags;
		e->map = map;

		(void)lib_rbInsert(&map->tree, &e->linkage);
	}

	return EOK;
}


static ph_map_t *phmap_mapOfAddr(addr_t addr)
{
	unsigned int i;

	for (i = 0; i < phmap_common.mapssz; i++) {
		if ((addr >= phmap_common.maps[i]->start) && (addr < phmap_common.maps[i]->stop)) {
			return phmap_common.maps[i];
		}
	}

	return NULL;
}


// TODO: should specify "whose" maps are allowed. At least kernel/proc_curent. could use page_flags & PAGE_OWNER_
// TODO: should allow to choose a particular map (could be other function, could export _phmap_alloc)
addr_t vm_phAlloc(size_t *size, page_flags_t page_flags, vm_flags_t vm_flags)
{
	ph_map_t *map;
	ph_entry_t *e;
	addr_t addr;
	unsigned int i;
	int res = -ENOMEM;

	*size = (*size + PH_ALIGN - 1U) & ~(PH_ALIGN - 1U);

	for (i = 0; i < phmap_common.mapssz; i++) {
		// TODO: check if map is allocable for current process / kernel

		/* TODO: check map attributes
		if (vm_flags){
			continue;
		}
		*/
		map = phmap_common.maps[i];

		(void)proc_lockSet(&map->lock);

		e = lib_treeof(ph_entry_t, linkage, map->tree.root);
		if ((e == NULL) || (map->stop - map->start < *size)) {
			(void)proc_lockClear(&map->lock);
			continue;
		}
		if ((e->lmaxgap < *size) && (e->rmaxgap < *size)) {
			(void)proc_lockClear(&map->lock);
			continue;
		}

		addr = 0U;
		res = _phmap_alloc(map, &addr, *size, page_flags, 0, NULL);
		(void)proc_lockClear(&map->lock);

		if (res == EOK) {
			return addr;
		}
	}

	if ((vm_flags & MAP_CONTIGUOUS) != 0U) {
		return PHADDR_INVALID;
	}
	// TODO: iterate over (allocable, acc. vm_flags to attr) maps and use one with any memory at all (can you do better? remember about locks)
	// TODO: update size accordingly to what was actually allocated

	return PHADDR_INVALID;
}


static void _vm_phmapEntrySplit(ph_map_t *m, ph_entry_t *e, ph_entry_t *new, size_t len)
{
	new->addr = e->addr + len;
	new->size = e->size - len;
	new->lmaxgap = 0;
	new->rmaxgap = e->rmaxgap;
	new->flags = e->flags;
	new->map = m;

	e->size = len;
	e->rmaxgap = 0;
	_phmap_augment(&e->linkage);

	(void)lib_rbInsert(&m->tree, &new->linkage);
}


int vm_phFree(addr_t addr, size_t size)
{
	ph_entry_t *e, *s;
	ph_entry_t t;
	size_t overlapSize;
	ptr_t overlapStart, overlapEnd;
	size_t overlapEOffset;
	int putEntry;
	ph_map_t *map;

	LIB_ASSERT_ALWAYS((addr & (PH_ALIGN - 1U)) == 0U, "phmap: vm_phFree called with unaligned address %p", (void *)addr);
	LIB_ASSERT_ALWAYS((size & (PH_ALIGN - 1U)) == 0U, "phmap: vm_phFree called with unaligned size %x", size);

	addr = (addr & ~(PH_ALIGN - 1U));
	size = (size + PH_ALIGN - 1U) & ~(PH_ALIGN - 1U);

	t.addr = addr;
	t.size = size;

	map = phmap_mapOfAddr(addr);
	if ((map == NULL) || (addr + size > map->stop)) {
		return -EINVAL;
	}

	(void)proc_lockSet(&map->lock);

	/* Region to unmap can span across multiple entries. */
	/* rbFind finds any entry having an overlap with the region in an unspecified order. */
	for (;;) {
		e = lib_treeof(ph_entry_t, linkage, lib_rbFind(&map->tree, &t.linkage));
		if (e == NULL) {
			break;
		}

		overlapStart = max((ptr_t)e->addr, addr);
		overlapEnd = min((ptr_t)e->addr + e->size, addr + size);
		overlapSize = (size_t)(overlapEnd - overlapStart);
		overlapEOffset = (size_t)(overlapStart - (ptr_t)e->addr);

		putEntry = 0;

		if ((ptr_t)e->addr == overlapStart) {
			if (e->size == overlapSize) {
				putEntry = 1;
			}
			else {
				e->addr += overlapSize;
				e->size -= overlapSize;
				e->lmaxgap += overlapSize;

				s = lib_treeof(ph_entry_t, linkage, lib_rbPrev(&e->linkage));
				if (s != NULL) {
					s->rmaxgap += overlapSize;
					_phmap_augment(&s->linkage);
				}

				_phmap_augment(&e->linkage);
			}
		}
		else if ((ptr_t)(e->addr + e->size) == overlapEnd) {
			e->size -= overlapSize;
			e->rmaxgap += overlapSize;

			s = lib_treeof(ph_entry_t, linkage, lib_rbNext(&e->linkage));
			if (s != NULL) {
				s->lmaxgap += overlapSize;
				_phmap_augment(&s->linkage);
			}

			_phmap_augment(&e->linkage);
		}
		else {
			s = phmap_entryalloc();
			/* This case if only possible if an unmapped region is in the middle of a single entry,
			 * so there is no possibility of partially unmapping. */
			if (s == NULL) {
				return -ENOMEM;
			}
			_vm_phmapEntrySplit(map, e, s, overlapEOffset);

			continue; /* Process in next iteration. */
		}

		if (putEntry != 0) {
			_phmap_entryfree(map, e);
		}
	}

	(void)proc_lockClear(&map->lock);

	return EOK;
}


static void _phmap_dump(rbnode_t *node)
{
	ph_entry_t *e = lib_treeof(ph_entry_t, linkage, node);
	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
	lib_printf("%p+%x, <%x, %x> (%x) [%x]", e->addr, e->size, e->lmaxgap, e->rmaxgap, e->allocsz, e->flags);
}

void phmap_dumpAll(void)
{
	unsigned int i;
	for (i = 0; i < phmap_common.mapssz; i++) {
		ph_map_t *map = phmap_common.maps[i];
		(void)proc_lockSet(&map->lock);
		lib_rbDump(map->tree.root, _phmap_dump);
		(void)proc_lockClear(&map->lock);
	}
}

int vm_mappages(pmap_t *pmap, void *vaddr, addr_t pa, size_t size, vm_attr_t attr)
{
	unsigned int i = 0;
	page_t tmp;
	page_t *ap;
	size_t s = SIZE_PAGE;

	LIB_ASSERT_ALWAYS(vaddr == NULL || (pa != PHADDR_INVALID && pa != 0U), "LOL %p %x", vaddr, pa);

	for (; i < size; i += SIZE_PAGE) {
		ap = NULL;
		while (pmap_enter(pmap, pa + i, vaddr + i, attr, ap) < 0) {
			tmp.addr = vm_phAlloc(&s, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, MAP_CONTIGUOUS);
			if (tmp.addr == PHADDR_INVALID) {
				return -ENOMEM;
			}
			ap = &tmp;
		}
	}

	return EOK;
}


#ifdef NOMMU
int _page_sbrk(pmap_t *pmap, void **start, void **end)
{
	size_t s = SIZE_PAGE;

	if (vm_mappages(pmap, (*end), (addr_t)(*end), s, PGHD_READ | PGHD_WRITE | PGHD_PRESENT) < 0) {
		return -ENOMEM;
	}

	(*end) += SIZE_PAGE;

	return EOK;
}
#else
int _page_sbrk(pmap_t *kpmap, void **start, void **end)
{
	page_t tmp, *ap = NULL;
	size_t s = SIZE_PAGE;
	addr_t addr;

	addr = vm_phAlloc(&s, PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP, MAP_CONTIGUOUS);
	if (addr == PHADDR_INVALID) {
		return -ENOMEM;
	}

	while (pmap_enter(kpmap, addr, (*end), PGHD_READ | PGHD_WRITE | PGHD_PRESENT, ap) < 0) {
		tmp.addr = vm_phAlloc(&s, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, MAP_CONTIGUOUS);
		if (tmp.addr == PHADDR_INVALID) {
			return -ENOMEM;
		}
		ap = &tmp;
	}

	(*end) += SIZE_PAGE;

	return EOK;
}
#endif


void vm_phGetStats(size_t *freesz)
{
	unsigned int i;
	ph_entry_t *e;
	*freesz = 0U;

	for (i = 0; i < phmap_common.mapssz; i++) {
		*freesz += phmap_common.maps[i]->stop - phmap_common.maps[i]->start;

		(void)proc_lockSet(&phmap_common.maps[i]->lock);
		e = lib_treeof(ph_entry_t, linkage, phmap_common.maps[i]->tree.root);
		if (e != NULL) {
			*freesz -= e->allocsz;
		}
		(void)proc_lockClear(&phmap_common.maps[i]->lock);
	}
}


void vm_phinfo(meminfo_t *info)
{
	// TODO;
}

/* TODO: replace map.c function (esp. for NOMMU)
ph_map_t *vm_getSharedMap(int map)
{
	ph_map_t *ret = NULL;

	if (map >= 0 && (size_t)map < phmap_common.mapssz) {
		ret = phmap_common.maps[map];
	}

	return ret;
}
*/


static int _phmap_mapsInit(pmap_t *kpmap, void **bss, void **top, ph_entry_t *initEntries, addr_t addr)
{
	size_t mapsCnt, id = 0;
	const syspage_map_t *map;
	ph_map_t *phmap;
	ph_entry_t *e;
	int err;

	mapsCnt = syspage_mapSize();
	if (mapsCnt == 0U) {
		return -EINVAL;
	}

	phmap_common.maps = (ph_map_t **)(*bss);
	// TODO: wrap into function (bootstrap-sbrk?)
#ifdef NOMMU
	size_t size;

	(*top) = max(((*bss) + (sizeof(ph_map_t *) + sizeof(ph_map_t)) * mapsCnt), (*top));
#else
	page_t page, ap, *p;

	while ((ptr_t)(*top) - (ptr_t)(*bss) < (ptr_t)((sizeof(ph_map_t *) + sizeof(ph_map_t)) * mapsCnt)) {
		do {
			// TODO: limit to maps for kernel data
			err = pmap_getPage(&page, &addr);
			LIB_ASSERT_ALWAYS((err == EOK), "vm: Problem with extending kernel heap for physical maps (vaddr=%p, %d)", *bss, err);
			LIB_ASSERT_ALWAYS(addr >= SIZE_PAGE, "vm: Problem with extending kernel heap for physical maps (vaddr=%p)", *bss);
		} while ((err != EOK) || ((page.flags & PAGE_FREE) == 0U));

		e = phmap_entryalloc();
		e->addr = page.addr;
		e->size = SIZE_PAGE;
		e->flags = PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP;
		e->next = initEntries;
		initEntries = e;

		p = NULL;
		while (pmap_enter(kpmap, page.addr, (*top), PGHD_READ | PGHD_WRITE | PGHD_PRESENT, p) < 0) {
			do {
				err = pmap_getPage(&ap, &addr);
				LIB_ASSERT_ALWAYS((err == EOK), "vm: Problem with extending kernel heap for physical maps (vaddr=%p, %d)", *bss, err);
				LIB_ASSERT_ALWAYS(addr >= SIZE_PAGE, "vm: Problem with extending kernel heap for ph_entry_t pool (vaddr=%p)", *bss);
			} while ((err != EOK) || ((page.flags & PAGE_FREE) == 0U));
			p = &ap;

			e = phmap_entryalloc();
			e->addr = ap.addr;
			e->size = SIZE_PAGE;
			e->flags = PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE;
			e->next = initEntries;
			initEntries = e;
		}

		(*top) += SIZE_PAGE;
	}
#endif

	phmap_common.mapssz = 0;
	phmap_common.maps = (ph_map_t **)(*bss);
	(*bss) += (sizeof(ph_map_t *) * mapsCnt);
	map = syspage_mapList();

	do {
		phmap_common.maps[id] = (*bss);
		phmap = phmap_common.maps[id];

		phmap->start = map->start;
		phmap->stop = map->end;
		(void)proc_lockInit(&phmap->lock, &proc_lockAttrDefault, "phmap.map");
		lib_rbInit(&phmap->tree, _phmap_cmp, _phmap_augment);

		phmap_common.mapssz += 1U;

#ifdef NOMMU
		const mapent_t *sysEntry = map->entries;
		if (sysEntry != NULL) {
			do {
				/* Skip temporary entries which are used only in phoenix-rtos-loader */
				if ((sysEntry->type == hal_entryTemp) || (sysEntry->end < phmap->start) || (sysEntry->start > phmap->stop)) {
					continue;
				}

				size = sysEntry->end - sysEntry->start;
				size = (size + PH_ALIGN - 1U) & ~(PH_ALIGN - 1U);
				err = _phmap_alloc(phmap, &addr, size, PAGE_OWNER_BOOT, MAP_FIXED, NULL);
				if (err != EOK) {
					return -ENOMEM;
				}
			} while ((sysEntry = sysEntry->next) != map->entries);
		}
#else
		addr = phmap->start;

		err = _phmap_alloc(phmap, &addr, phmap->stop - phmap->start, 0, MAP_FIXED, NULL);
		if (err != EOK) {
			return err;
		}

		for (;;) {
			err = pmap_getPage(&page, &addr);
			if ((err == -ENOMEM) || (addr >= phmap->stop)) {
				break;
			}

			if (phmap->stop - SIZE_PAGE < addr) {  // TODO: handle page at MAX_ADDR-SIZE_PAGE
				break;
			}
			LIB_ASSERT_ALWAYS(vm_phFree(page.addr, SIZE_PAGE) == EOK, "vm: Problem with freeing page during phmap init (addr=%p)", (void *)page.addr);

			if ((err == EOK) && ((page.flags & PAGE_FREE) == 0U)) {
				err = _phmap_alloc(phmap, &page.addr, SIZE_PAGE, page.flags, MAP_FIXED, NULL);
				if (err != EOK) {
					return err;
				}
			}

			/* Wrap over 0 */
			if (addr < SIZE_PAGE) {
				break;
			}
		}
#endif

		(*bss) += sizeof(ph_map_t);
		++id;
	} while ((map = map->next) != syspage_mapList());

	/* return to the pool initEntries used as markers of pages for bootstrapping */
	while (initEntries != NULL) {
		e = initEntries;
		initEntries = e->next;

		phmap = phmap_mapOfAddr(e->addr);
		if ((phmap == NULL) || (e->addr + SIZE_PAGE > phmap->stop)) {  // TODO: should limit to kernel-maps
			return -EINVAL;
		}
		err = _phmap_alloc(phmap, &e->addr, SIZE_PAGE, e->flags, MAP_FIXED, NULL);
		if (err != EOK) {
			return err;
		}

		e->next = phmap_common.free;
		e->addr = PHADDR_INVALID;
		phmap_common.free = e;
	}

	return EOK;
}


void _vm_phmap_init(pmap_t *kpmap, void **bss, void **top)
{
	int result;
	unsigned int i;
	ph_entry_t *e;
	size_t poolsz, freesz = 0U, allocsz = 0U;
	addr_t addr = 0U;

	/* FIXME: unify information about available/busy pages among MMU/NOMMU */
#ifdef NOMMU
	const syspage_map_t *map = syspage_mapList();
	const mapent_t *ent;
	LIB_ASSERT_ALWAYS(map != NULL, "vm: No syspage maps found!");
	do {
		ent = map->entries;
		freesz += (map->end - map->start);
		while (ent != NULL) {
			if (ent->type == hal_entryTemp) {
				continue;
			}
			freesz -= (ent->end - ent->start);
			allocsz += (ent->end - ent->start);
			ent = ent->next;
		}
		map = map->next;
	} while (map != syspage_mapList());
#else
	int err;
	page_t page, ap, *p;
	size_t s;
	void *vaddr;

	addr = 0U;

	for (;;) {
		err = pmap_getPage(&page, &addr);
		if (err == -ENOMEM) {
			break;
		}

		if (err == EOK) {
			if ((page.flags & PAGE_FREE) != 0U) {
				freesz += SIZE_PAGE;
			}
			else {
				if (((page.flags >> 1) & 7U) == PAGE_OWNER_BOOT) {  // TODO: why is it like that??? (>>1)&7U
					phmap_common.bootsz += SIZE_PAGE;
				}
				allocsz += SIZE_PAGE;
			}
		}

		/* Wrap over 0 */
		if (addr < SIZE_PAGE) {
			break;
		}
	}

#endif

	(void)proc_lockInit(&phmap_common.lock, &proc_lockAttrDefault, "phmap.common");

	/* Init map entry pool */
	phmap_common.ntotal = freesz / (10U * SIZE_PAGE + sizeof(ph_entry_t));
	phmap_common.nfree = phmap_common.ntotal;

	// TODO: what happens with kernel segments? (pmap_segment called in map.c init) - probably solved by initEntries + boot pages from getPage?

	e = NULL;
#ifdef NOMMU
	phmap_common.free = (*bss);
	(*top) = (void *)max((ptr_t)*top, (ptr_t)(*bss) + sizeof(ph_entry_t) * phmap_common.nfree);
#else
	addr = 0;

	while ((ptr_t)(*top) - (ptr_t)(*bss) < (ptr_t)sizeof(ph_entry_t) * (ptr_t)phmap_common.ntotal) {
		do {
			// TODO: limit to maps for kernel data
			err = pmap_getPage(&page, &addr);
			LIB_ASSERT_ALWAYS(err != -ENOMEM, "vm: Problem with extending kernel heap for ph_entry_t pool (vaddr=%p)", *bss);
			LIB_ASSERT_ALWAYS(addr >= SIZE_PAGE, "vm: Problem with extending kernel heap for ph_entry_t pool (vaddr=%p)", *bss);
		} while ((err != EOK) || ((page.flags & PAGE_FREE) == 0U));

		if (e == NULL) {
			e = (*bss);
		}
		else {
			e->next = e + 1;
			e = e->next;
		}
		e->addr = page.addr;
		e->size = SIZE_PAGE;
		e->flags = PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP;
		e->next = NULL;
		phmap_common.nfree--;

		p = NULL;
		while (pmap_enter(kpmap, page.addr, (*top), PGHD_READ | PGHD_WRITE | PGHD_PRESENT, p) < 0) {
			do {
				err = pmap_getPage(&ap, &addr);
				LIB_ASSERT_ALWAYS(err != -ENOMEM, "vm: Problem with extending kernel heap for ph_entry_t pool (vaddr=%p)", *bss);
				LIB_ASSERT_ALWAYS(addr >= SIZE_PAGE, "vm: Problem with extending kernel heap for ph_entry_t pool (vaddr=%p)", *bss);
			} while ((err != EOK) || ((page.flags & PAGE_FREE) == 0U));
			p = &ap;

			e->next = e + 1;
			e = e->next;
			e->addr = ap.addr;
			e->size = SIZE_PAGE;
			e->flags = PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE;
			e->next = NULL;
			phmap_common.nfree--;
		}

		(*top) += SIZE_PAGE;
	}
	phmap_common.free = (e != NULL) ? (e + 1) : (*bss);
#endif
	poolsz = min((ptr_t)(*top) - (ptr_t)(*bss), sizeof(ph_entry_t) * phmap_common.ntotal);

	for (i = 0; i < phmap_common.nfree - 1U; ++i) {
		phmap_common.free[i].next = phmap_common.free + i + 1U;
		phmap_common.free[i].addr = PHADDR_INVALID;
	}

	phmap_common.free[i].next = NULL;

	if (e != NULL) {
		e = (*bss);
	}

	(*bss) += poolsz;

	result = _phmap_mapsInit(kpmap, bss, top, e, addr);
	LIB_ASSERT_ALWAYS(result >= 0, "vm: Problem with physical maps initialization (%d).", result);

	vm_phGetStats(&freesz);

#ifndef NOMMU
	/* Initialize kernel space for user processes */
	s = SIZE_PAGE;
	p = NULL;
	vaddr = (*top);
	while (_pmap_kernelSpaceExpand(kpmap, &vaddr, (*top) + max((freesz + allocsz) / 4U, (1UL << 23)), p) != 0) {
		page.addr = vm_phAlloc(&s, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE, MAP_CONTIGUOUS);
		if (page.addr == PHADDR_INVALID) {
			return;
		}
		p = &page;
	}
#endif

	/* Show statistics on the console */
	lib_printf("vm: Initializing page allocator (%d+%d)/%dKB, page_t=%d\n", (allocsz - phmap_common.bootsz) / 1024U,
			phmap_common.bootsz / 1024U, (freesz + allocsz) / 1024U, sizeof(page_t));
	lib_printf("vm: Initializing physical memory allocator: (%d*%d) %d\n", phmap_common.nfree, sizeof(ph_entry_t), poolsz);

	/* Create NULL pointer entry */
	result = vm_mappages(kpmap, NULL, 0, SIZE_PAGE, PGHD_USER | ~PGHD_PRESENT);
	LIB_ASSERT_ALWAYS(result >= 0, "vm: Problem with mapping NULL page (%d).", result);

	// TODO: adjust dumping
	// _page_showPages();

	// phmap_dumpAll();
}
