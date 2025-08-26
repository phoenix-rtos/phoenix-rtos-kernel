/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - memory mapper
 *
 * Copyright 2016 Phoenix Systems
 * Author: Pawel Pisarczyk, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <stddef.h>

#include "hal/hal.h"
#include "lib/lib.h"
#include "proc/proc.h"
#include "include/errno.h"
#include "include/signal.h"
#include "syspage.h"
#include "map.h"
#include "amap.h"


extern unsigned int __bss_start;

static struct {
	vm_map_t *kmap;
	vm_object_t *kernel;

	lock_t lock;

	unsigned int ntotal, nfree;
	map_entry_t *free;
	map_entry_t *entries;

	vm_map_t **maps;
	size_t mapssz;
} map_common;


static map_entry_t *map_alloc(void);


static map_entry_t *map_allocN(int n);


void map_free(map_entry_t *entry);


static int _map_force(vm_map_t *map, map_entry_t *e, void *paddr, unsigned int prot);


static int map_cmp(rbnode_t *n1, rbnode_t *n2)
{
	map_entry_t *e1 = lib_treeof(map_entry_t, linkage, n1);
	map_entry_t *e2 = lib_treeof(map_entry_t, linkage, n2);

	if (e2->vaddr + e2->size <= e1->vaddr) {
		return 1;
	}

	if (e1->vaddr + e1->size <= e2->vaddr) {
		return -1;
	}

	return 0;
}


static void map_augment(rbnode_t *node)
{
	rbnode_t *it;
	map_entry_t *n = lib_treeof(map_entry_t, linkage, node);
	map_entry_t *p = n;

	if (node->left == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(map_entry_t, linkage, it->parent);
			if (it->parent->right == it) {
				break;
			}
		}

		n->lmaxgap = (n->vaddr <= p->vaddr) ? ((size_t)n->vaddr - (size_t)n->map->start) : ((size_t)n->vaddr - (size_t)p->vaddr) - p->size;
	}
	else {
		map_entry_t *l = lib_treeof(map_entry_t, linkage, node->left);
		n->lmaxgap = max(l->lmaxgap, l->rmaxgap);
	}

	if (node->right == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(map_entry_t, linkage, it->parent);
			if (it->parent->left == it) {
				break;
			}
		}

		n->rmaxgap = (n->vaddr >= p->vaddr) ? ((size_t)n->map->stop - (size_t)n->vaddr) - n->size : ((size_t)p->vaddr - (size_t)n->vaddr) - n->size;
	}
	else {
		map_entry_t *r = lib_treeof(map_entry_t, linkage, node->right);
		n->rmaxgap = max(r->lmaxgap, r->rmaxgap);
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(map_entry_t, linkage, it);
		p = lib_treeof(map_entry_t, linkage, it->parent);

		if (it->parent->left == it) {
			p->lmaxgap = max(n->lmaxgap, n->rmaxgap);
		}
		else {
			p->rmaxgap = max(n->lmaxgap, n->rmaxgap);
		}
	}
}


void map_dump(rbnode_t *node)
{
	map_entry_t *e = lib_treeof(map_entry_t, linkage, node);
	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)lib_printf("%p+%x, %x, %x", e->vaddr, e->size, e->lmaxgap, e->rmaxgap);
}


static int _map_add(process_t *p, vm_map_t *map, map_entry_t *entry)
{
#ifdef NOMMU
	if (p != NULL) {
		/* MISRA Rule 17.7: Unused returned value, (void) added in lines 135, 137*/
		(void)proc_lockSet(&p->lock);
		LIST_ADD(&p->entries, entry);
		(void)proc_lockClear(&p->lock);
	}
	entry->process = p;
#endif

	entry->map = map;
	return lib_rbInsert(&map->tree, &entry->linkage);
}


static void _map_remove(vm_map_t *map, map_entry_t *entry)
{
#ifdef NOMMU
	process_t *p = entry->process;
	if (p != NULL) {
		/* MISRA Rule 17.7: Unused returned value, (void) added in lines 153, 155*/
		(void)proc_lockSet(&p->lock);
		LIST_REMOVE(&p->entries, entry);
		(void)proc_lockClear(&p->lock);
	}
	entry->process = NULL;
#endif

	lib_rbRemove(&map->tree, &entry->linkage);
	entry->map = NULL;
}


static void _entry_put(vm_map_t *map, map_entry_t *e)
{
	amap_put(e->amap);
	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)vm_objectPut(e->object);
	_map_remove(map, e);
	map_free(e);
}


void *_map_find(vm_map_t *map, void *vaddr, size_t size, map_entry_t **prev, map_entry_t **next)
{
	map_entry_t *e = lib_treeof(map_entry_t, linkage, map->tree.root);

	*prev = NULL;
	*next = NULL;

	if (((void *)map->stop - size) < vaddr) {
		return NULL;
	}
	if (vaddr < map->start) {
		vaddr = map->start;
	}

	while (e != NULL) {

		if ((size <= e->lmaxgap) && ((vaddr + size) <= e->vaddr)) {
			*next = e;

			if (e->linkage.left == NULL) {
				return max(vaddr, e->vaddr - e->lmaxgap);
			}
			e = lib_treeof(map_entry_t, linkage, e->linkage.left);
			continue;
		}

		if ((size <= e->rmaxgap) /*&& (vaddr + size) <= (e->vaddr + e->size + e->rmaxgap)*/) {
			*prev = e;

			if (e->linkage.right == NULL) {
				return max(vaddr, e->vaddr + e->size);
			}
			e = lib_treeof(map_entry_t, linkage, e->linkage.right);
			continue;
		}

		for (;; e = lib_treeof(map_entry_t, linkage, e->linkage.parent)) {
			if (e->linkage.parent == NULL) {
				return NULL;
			}

			if ((e == lib_treeof(map_entry_t, linkage, e->linkage.parent->left)) && ((lib_treeof(map_entry_t, linkage, e->linkage.parent)->rmaxgap >= size))) {
				break;
			}
		}
		e = lib_treeof(map_entry_t, linkage, e->linkage.parent);

		for (*next = e; (*next)->linkage.parent != NULL; *next = lib_treeof(map_entry_t, linkage, (*next)->linkage.parent))
			if ((*next) == lib_treeof(map_entry_t, linkage, (*next)->linkage.parent->left)) {
				break;
			}

		*next = lib_treeof(map_entry_t, linkage, (*next)->linkage.parent);

		*prev = e;
		if (e->linkage.right == NULL) {
			return e->vaddr + e->size;
		}

		e = lib_treeof(map_entry_t, linkage, e->linkage.right);
	}

	return vaddr;
}


static void *_map_map(vm_map_t *map, void *vaddr, process_t *proc, size_t size, u8 prot, vm_object_t *o, off_t offs, u8 flags, map_entry_t **entry)
{
	void *v;
	map_entry_t *prev, *next, *e;
	unsigned int lmerge, rmerge;
	amap_t *amap;

#ifdef NOMMU
	if (o == VM_OBJ_PHYSMEM) {
		/* MISRA Rule 11.6: (unsigned int *) added*/
		return (void *)(unsigned int *)(ptr_t)offs;
	}
#endif

	if ((v = _map_find(map, vaddr, size, &prev, &next)) == NULL) {
		return NULL;
	}

	rmerge = (next != NULL && v + size == next->vaddr && next->object == o && next->flags == flags && next->prot == prot && next->protOrig == prot) ? 1U : 0U;
	lmerge = (prev != NULL && v == prev->vaddr + prev->size && prev->object == o && prev->flags == flags && prev->prot == prot && prev->protOrig == prot) ? 1U : 0U;

	if (offs != -1) {
		if (((u64)offs & (SIZE_PAGE - 1UL)) != 0UL) {
			return NULL;
		}

		if (rmerge != 0U) {
			rmerge &= (next->offs == offs + (s64)size) ? 1U : 0U;
		}

		if (lmerge != 0U) {
			lmerge &= (offs == prev->offs + (s64)prev->size) ? 1U : 0U;
		}
	}

#ifdef NOMMU
	rmerge = (rmerge != 0U && (proc == next->process)) ? 1U : 0U;
	lmerge = (lmerge != 0U && (proc == prev->process)) ? 1U : 0U;
#endif

#if 1
	if (o == NULL) {
		if (lmerge != 0U && rmerge != 0U && (next->amap == prev->amap)) {
			/* Both use the same amap, can merge */
		}
		else {
			/* Can't merge to the left if amap array size is too small */
			if (lmerge != 0U && (amap = prev->amap) != NULL && (amap->size * SIZE_PAGE - (size_t)prev->aoffs - prev->size) < size) {
				lmerge = 0;
			}
			/* Can't merge to the right if amap offset is too small */
			if (rmerge != 0U && (amap = next->amap) != NULL && (size_t)next->aoffs < size) {
				rmerge = 0;
			}
			/* amaps differ, we can only merge one way */
			if (lmerge != 0U && rmerge != 0U) {
				rmerge = 0;
			}
		}
	}
#else
	/* Disable merging of anonymous entries */
	if (o == NULL)
		rmerge = lmerge = 0;
#endif

	if (rmerge != 0U && lmerge != 0U) {
		e = prev;
		e->size += size + next->size;
		e->rmaxgap = next->rmaxgap;

		map_augment(&e->linkage);
		_entry_put(map, next);
	}
	else if (rmerge != 0U) {
		e = next;
		e->vaddr = v;
		e->offs = offs;
		e->size += size;
		e->lmaxgap -= size;

		if (e->aoffs != 0) {
			e->aoffs -= (int)size;
		}

		if (prev != NULL) {
			prev->rmaxgap -= size;
			map_augment(&prev->linkage);
		}

		map_augment(&e->linkage);
	}
	else if (lmerge) {
		e = prev;
		e->size += size;
		e->rmaxgap -= size;

		if (next != NULL) {
			next->lmaxgap -= size;
			map_augment(&next->linkage);
		}

		map_augment(&e->linkage);
	}
	else {
		if ((e = map_alloc()) == NULL) {
			return NULL;
		}

		e->vaddr = v;
		e->size = size;
		e->object = vm_objectRef(o);
		e->offs = offs;
		e->flags = flags;
		e->prot = prot;
		e->protOrig = prot;

		e->amap = NULL;
		e->aoffs = 0;

		if (o == NULL) {
			/* Try to use existing amap */
			if (next != NULL && next->amap != NULL && e->vaddr >= (next->vaddr - next->aoffs)) {
				e->amap = amap_ref(next->amap);
				e->aoffs = next->aoffs - (next->vaddr - e->vaddr);
			}
			else if (prev != NULL && prev->amap != NULL && (SIZE_PAGE * prev->amap->size - (size_t)prev->aoffs + (size_t)prev->vaddr) >= ((size_t)e->vaddr + size)) {
				e->amap = amap_ref(prev->amap);
				e->aoffs = prev->aoffs + (e->vaddr - prev->vaddr);
			}
		}

		/* MISRA Rule 17.7: Unused returned value, (void) added */
		(void)_map_add(proc, map, e);
	}

	/* Clear anon entries */
	if (e->amap != NULL) {
		amap_clear(e->amap, (size_t)e->aoffs + ((size_t)v - (size_t)e->vaddr), size);
	}
	if (entry != NULL) {
		*entry = e;
	}

	return v;
}


void *vm_mapFind(vm_map_t *map, void *vaddr, size_t size, u8 flags, u8 prot)
{
	/* MISRA Rule 17.7: Unused returned value, (void) added 392, 394 */
	(void)proc_lockSet(&map->lock);
	vaddr = _map_map(map, vaddr, NULL, size, prot, map_common.kernel, -1, flags, NULL);
	(void)proc_lockClear(&map->lock);

	return vaddr;
}


static void vm_mapEntryCopy(map_entry_t *dst, map_entry_t *src, int refAnons)
{
	hal_memcpy(dst, src, sizeof(map_entry_t));
	src->amap = amap_ref(dst->amap);
	/* In case of splitting the entry the anons shouldn't be reffed as they just change the owner. */
	if (refAnons != 0) {
		amap_getanons(dst->amap, dst->aoffs, (int)dst->size);
	}
	src->object = vm_objectRef(dst->object);
}


static void vm_mapEntrySplit(process_t *p, vm_map_t *m, map_entry_t *e, map_entry_t *new, size_t len)
{
	vm_mapEntryCopy(new, e, 0);

	new->vaddr += len;
	new->size -= len;
	new->aoffs += (int)len;
	new->offs = (new->offs == -1) ? -1 : (new->offs + (s64)len);
	new->lmaxgap = 0;

	e->size = len;
	e->rmaxgap = 0;
	map_augment(&e->linkage);

	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)_map_add(p, m, new);
}


int _vm_munmap(vm_map_t *map, void *vaddr, size_t size)
{
	map_entry_t *e, *s;
	map_entry_t t;
	process_t *proc = proc_current()->process;
	size_t overlapEOffset;
	size_t overlapSize;
	ptr_t overlapStart, overlapEnd;
	ptr_t eAoffs;
	int putEntry;

	/* MISRA Rule 11.6: (unsigned int *) added*/
	if (((size & (SIZE_PAGE - 1U)) != 0U) || (((ptr_t)(unsigned int *)vaddr & (SIZE_PAGE - 1U)) != 0U)) {
		return -EINVAL;
	}

	t.vaddr = vaddr;
	t.size = size;

	/* Region to unmap can span across multiple entries. */
	/* rbFind finds any entry having an overlap with the region in an unspecified order. */
	for (;;) {
		e = lib_treeof(map_entry_t, linkage, lib_rbFind(&map->tree, &t.linkage));
		if (e == NULL) {
			break;
		}

#ifdef NOMMU
		proc = e->process;
#endif

		overlapStart = max((ptr_t)e->vaddr, (ptr_t)vaddr);
		overlapEnd = min((ptr_t)e->vaddr + e->size, (ptr_t)vaddr + size);
		overlapSize = (size_t)(overlapEnd - overlapStart);
		/* MISRA Rule 11.6: (unsigned int *) added*/
		overlapEOffset = (size_t)(overlapStart - (ptr_t)(unsigned int *)e->vaddr);
		eAoffs = (unsigned int)e->aoffs;

		putEntry = 0;

		/* MISRA Rule 11.6: (unsigned int *) added*/
		if ((ptr_t)(unsigned int *)e->vaddr == overlapStart) {
			if (e->size == overlapSize) {
				putEntry = 1;
			}
			else {
				e->aoffs += (int)overlapSize;
				e->offs = (e->offs == -1) ? -1 : (e->offs + (s64)overlapSize);
				e->vaddr += overlapSize;
				e->size -= overlapSize;
				e->lmaxgap += overlapSize;

				s = lib_treeof(map_entry_t, linkage, lib_rbPrev(&e->linkage));
				if (s != NULL) {
					s->rmaxgap += overlapSize;
					map_augment(&s->linkage);
				}

				map_augment(&e->linkage);
			}
		}
		/* MISRA Rule 11.6: (unsigned int *) added*/
		else if ((ptr_t)(unsigned int *)(e->vaddr + e->size) == overlapEnd) {
			e->size -= overlapSize;
			e->rmaxgap += overlapSize;

			s = lib_treeof(map_entry_t, linkage, lib_rbNext(&e->linkage));
			if (s != NULL) {
				s->lmaxgap += overlapSize;
				map_augment(&s->linkage);
			}

			map_augment(&e->linkage);
		}
		else {
			s = map_alloc();
			/* This case if only possible if an unmapped region is in the middle of a single entry,
			 * so there is no possibility of partially unmapping. */
			if (s == NULL) {
				return -ENOMEM;
			}
			vm_mapEntrySplit(proc, map, e, s, overlapEOffset);

			continue; /* Process in next iteration. */
		}

		/* Perform amap and pmap changes only when we are sure we have enough space to perform corresponding map changes. */

		/* Note: what if NEEDS_COPY? */
		/* TODO: Offset s 64 bits in size and we perform cast to int check if we don't lose info */
		amap_putanons(e->amap, (int)eAoffs + (int)overlapEOffset, (int)overlapSize);

		/* MISRA Rule 11.6: (unsigned int *) added x2*/
		/* MISRA Rule 17.7: Unused returned value, (void) added */
		(void)pmap_remove(&map->pmap, (void *)(unsigned int *)overlapStart, (void *)(unsigned int *)overlapEnd);

		if (putEntry != 0) {
			_entry_put(map, e);
		}
	}

	return EOK;
}


unsigned vm_flagsToAttr(unsigned flags)
{
	unsigned attr = 0;
	if ((flags & MAP_UNCACHED) != 0U) {
		attr |= PGHD_NOT_CACHED;
	}
	if ((flags & MAP_DEVICE) != 0U) {
		attr |= PGHD_DEV;
	}

	return attr;
}


static unsigned vm_protToAttr(unsigned prot)
{
	unsigned attr = 0;

	if ((prot & PROT_READ) != 0U) {
		attr |= (PGHD_READ | PGHD_PRESENT);
	}
	if ((prot & PROT_WRITE) != 0U) {
		attr |= (PGHD_WRITE | PGHD_PRESENT);
	}
	if ((prot & PROT_EXEC) != 0U) {
		attr |= PGHD_EXEC;
	}
	if ((prot & PROT_USER) != 0U) {
		attr |= PGHD_USER;
	}

	return attr;
}


void *_vm_mmap(vm_map_t *map, void *vaddr, page_t *p, size_t size, u8 prot, vm_object_t *o, off_t offs, u8 flags)
{
	unsigned attr;
	void *w;
	process_t *process = NULL;
	thread_t *current;
	map_entry_t *e;

	if ((size == 0U) || ((size & (SIZE_PAGE - 1U)) != 0U)) {
		return NULL;
	}

	if ((flags & MAP_FIXED) != 0U) {
		if (_vm_munmap(map, vaddr, size) < 0) {
			return NULL;
		}
	}

	/* NULL page indicates that proc sybsystem is ready */
	if (p == NULL && (current = proc_current()) != NULL) {
		process = current->process;
	}
	else if (p != NULL && p->idx != 0U) {
		size = 1UL << p->idx;
	}

	if ((vaddr = _map_map(map, vaddr, process, size, prot, o, offs, flags, &e)) == NULL) {
		return NULL;
	}

	if (p != NULL) {
		attr = vm_protToAttr(prot) | vm_flagsToAttr(flags);

		for (w = vaddr; w < (vaddr + size); w += SIZE_PAGE) {
			/* MISRA Rule 17.7: Unused returned value, (void) added */
			(void)page_map(&map->pmap, w, (p++)->addr, attr);
		}

		return vaddr;
	}

	if (process != NULL && process->lazy != 0U) {
		return vaddr;
	}

	for (w = vaddr; w < vaddr + size; w += SIZE_PAGE) {
		if (_map_force(map, e, w, prot) != 0) {
			amap_putanons(e->amap, e->aoffs, w - vaddr);

			/* MISRA Rule 11.6: (unsigned int *) added x2 */
			/* MISRA Rule 17.7: Unused returned value, (void) added */
			(void)pmap_remove(&map->pmap, vaddr, (void *)(unsigned int *)((ptr_t)(unsigned int *)w + SIZE_PAGE));

			_entry_put(map, e);
			return NULL;
		}
	}

	return vaddr;
}


void *vm_mmap(vm_map_t *map, void *vaddr, page_t *p, size_t size, u8 prot, vm_object_t *o, off_t offs, u8 flags)
{
	if (map == NULL) {
		map = map_common.kmap;
	}

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 639, 641*/
	(void)proc_lockSet(&map->lock);
	vaddr = _vm_mmap(map, vaddr, p, size, prot, o, offs, flags);
	(void)proc_lockClear(&map->lock);
	return vaddr;
}


/*
 * Fault routines
 */

int vm_lockVerify(vm_map_t *map, amap_t **amap, vm_object_t *o, void *vaddr, off_t offs)
{
	map_entry_t t, *e;

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 655, 671*/
	(void)proc_lockSet(&map->lock);

	t.vaddr = vaddr;
	t.size = SIZE_PAGE;

	e = lib_treeof(map_entry_t, linkage, lib_rbFind(&map->tree, &t.linkage));

	if (e == NULL || e->object != o || (amap != NULL && e->amap != *amap) /* More checks? */) {
		if (amap != NULL) {
			*amap = NULL;
		}

		return -EINVAL;
	}

	if (amap != NULL) {
		(void)proc_lockSet(&(*amap)->lock);
	}

	return EOK;
}


int vm_mapFlags(vm_map_t *map, void *vaddr)
{
	unsigned flags;
	map_entry_t t, *e;

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 684, 692, 697*/
	(void)proc_lockSet(&map->lock);

	t.vaddr = vaddr;
	t.size = SIZE_PAGE;

	e = lib_treeof(map_entry_t, linkage, lib_rbFind(&map->tree, &t.linkage));

	if (e == NULL) {
		(void)proc_lockClear(&map->lock);
		return -EFAULT;
	}

	flags = e->flags & ~MAP_NEEDSCOPY;
	(void)proc_lockClear(&map->lock);

	return (int)flags;
}


int vm_mapForce(vm_map_t *map, void *paddr, unsigned prot)
{
	map_entry_t t, *e;
	int err;

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 709, 717, 722*/
	(void)proc_lockSet(&map->lock);

	t.vaddr = paddr;
	t.size = SIZE_PAGE;

	e = lib_treeof(map_entry_t, linkage, lib_rbFind(&map->tree, &t.linkage));

	if (e == NULL) {
		(void)proc_lockClear(&map->lock);
		return -EFAULT;
	}

	err = _map_force(map, e, paddr, prot);
	(void)proc_lockClear(&map->lock);
	return err;
}


static unsigned map_checkProt(unsigned int baseProt, unsigned int newProt)
{
	return (baseProt | newProt) ^ baseProt;
}


static int _map_force(vm_map_t *map, map_entry_t *e, void *paddr, unsigned int prot)
{
	unsigned int attr;
	int offs;
	page_t *p = NULL;
	int flagsCheck = (int)map_checkProt(e->prot, prot);

	if (flagsCheck != 0) {
		return flagsCheck;
	}
	if (((((unsigned int)prot & PROT_WRITE) != 0U) && ((e->flags & MAP_NEEDSCOPY) != 0U)) || ((e->object == NULL) && (e->amap == NULL))) {
		e->amap = amap_create(e->amap, &e->aoffs, e->size);
		if (e->amap == NULL) {
			return -ENOMEM;
		}

		e->flags &= ~MAP_NEEDSCOPY;
	}

	offs = (paddr - e->vaddr);

	if (e->amap == NULL) {
		p = vm_objectPage(map, NULL, e->object, paddr, ((e->offs < 0) ? e->offs : (e->offs + offs)));
	}
	else { /* if (e->object != VM_OBJ_PHYSMEM) FIXME disabled until memory objects are created for syspage progs */
		p = amap_page(map, e->amap, e->object, paddr, e->aoffs + offs, ((e->offs < 0) ? e->offs : (e->offs + offs)), prot);
	}

	attr = vm_protToAttr(prot) | vm_flagsToAttr(e->flags);

	if ((p == NULL) && (e->object == VM_OBJ_PHYSMEM)) {
		/* TODO: Offset s 64 bits in size and we perform cast to int check if we don't lose info */
		if (page_map(&map->pmap, paddr, (addr_t)e->offs + (addr_t)offs, attr) < 0) {
			return -ENOMEM;
		}
	}
	else if (p == NULL) {
		return -ENOMEM;
	}
	else if (page_map(&map->pmap, paddr, p->addr, attr) < 0) {
		amap_putanons(e->amap, e->aoffs + offs, (int)SIZE_PAGE);
		return -ENOMEM;
	}

	return EOK;
}


#ifndef NOMMU
static void map_pageFault(unsigned int n, exc_context_t *ctx)
{
	thread_t *thread;
	vm_map_t *map;
	void *vaddr, *paddr;
	int prot;

	prot = hal_exceptionsFaultType(n, ctx);
	vaddr = hal_exceptionsFaultAddr(n, ctx);
	paddr = (void *)((unsigned long)vaddr & ~(SIZE_PAGE - 1));

#ifdef PAGEFAULTSTOP
	process_dumpException(n, ctx);
	/* clang-format off */
	__asm__ volatile ("1: b 1b");
	/* clang-format on */
#endif

	if (hal_exceptionsPC(ctx) >= VADDR_KERNEL) /* output exception ASAP to avoid being deadlocked on spinlock */
		process_dumpException(n, ctx);

	hal_cpuEnableInterrupts();

	thread = proc_current();

	if (thread->process != NULL && !pmap_belongs(&map_common.kmap->pmap, vaddr))
		map = thread->process->mapp;
	else
		map = map_common.kmap;

	if (vm_mapForce(map, paddr, prot)) {
		process_dumpException(n, ctx);

		if (thread->process == NULL) {
			hal_cpuDisableInterrupts();
			hal_cpuHalt();
		}

		threads_sigpost(thread->process, thread, signal_segv);
	}
}
#endif


int vm_munmap(vm_map_t *map, void *vaddr, size_t size)
{
	int result;

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 829, 831*/
	(void)proc_lockSet(&map->lock);
	result = _vm_munmap(map, vaddr, size);
	(void)proc_lockClear(&map->lock);

	return result;
}


int vm_mprotect(vm_map_t *map, void *vaddr, size_t len, int prot)
{
	int result = EOK;
	void *currVaddr;
	size_t lenLeft = len, currSize, needed;
	process_t *p = proc_current()->process;
	addr_t pa;
	unsigned int attr;
	int needscopyNonLazy;
	map_entry_t *e, *buf = NULL, *prev;
	map_entry_t t;

	/* MISRA Rule 11.6: (unsigned int *) added */
	if (((((ptr_t)(unsigned int *)vaddr) & (SIZE_PAGE - 1U)) != 0U) || (len == 0U) || ((len & (SIZE_PAGE - 1U)) != 0U)) {
		return -EINVAL;
	}

	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)proc_lockSet(&map->lock);

	/* Validate */

	t.size = SIZE_PAGE;
	t.vaddr = vaddr;

	needed = 0;
	do {
		e = lib_treeof(map_entry_t, linkage, lib_rbFind(&map->tree, &t.linkage));
		if (e == NULL) {
			result = -ENOMEM;
			break;
		}
		if (map_checkProt(e->protOrig, (unsigned int)prot) != 0U) {
			result = -EACCES;
			break;
		}

		currSize = e->size;
		/* First entry may not be aligned. */
		if (e->vaddr < t.vaddr) {
			currSize -= ((unsigned int)t.vaddr - (unsigned int)e->vaddr);
			needed++;
		}
		/* Last entry may not be changed fully. */
		if (lenLeft < currSize) {
			needed++;
		}
		lenLeft -= min(lenLeft, currSize);
		t.vaddr += currSize;
	} while (lenLeft != 0U);

	if ((result == EOK) && (needed != 0U)) {
		buf = map_allocN((int)needed);
		if (buf == NULL) {
			result = -ENOMEM;
		}
	}

	if (result == EOK) {
		t.vaddr = vaddr;
		prev = NULL;
		lenLeft = len;
		do {
			e = lib_treeof(map_entry_t, linkage, lib_rbFind(&map->tree, &t.linkage));

			if (prev == NULL) {
				/* First entry */
				if (e->vaddr < t.vaddr) {
					/* Split */
					prev = e;

					e = buf;
					buf = buf->next;

					vm_mapEntrySplit(p, map, prev, e, (size_t)t.vaddr - (size_t)prev->vaddr);
				}
			}
			else if ((prev->protOrig == e->protOrig) && (prev->object == e->object) && (prev->flags == e->flags)) {
				/* Merge */
				prev->rmaxgap = e->rmaxgap;
				prev->size += e->size;

				_entry_put(map, e);

				map_augment(&prev->linkage);
				e = prev;
			}

			if (lenLeft < e->size) {
				vm_mapEntrySplit(p, map, e, buf, lenLeft);
			}

			/* TODO: int cast to char - check if we don't lose info*/
			e->prot = (unsigned char)prot;

			attr = (vm_protToAttr(e->prot) | vm_flagsToAttr(e->flags));
			needscopyNonLazy = 0;
			/* If an entry needs copy, enter it as a readonly to copy it on first access. */
			if ((e->flags & MAP_NEEDSCOPY) != 0U) {
				if ((p == NULL) || (p->lazy == 0U)) {
					needscopyNonLazy = 1;
				}
				else {
					attr &= ~PGHD_WRITE;
				}
			}
			for (currVaddr = e->vaddr; currVaddr < (e->vaddr + e->size); currVaddr += SIZE_PAGE) {
				if (needscopyNonLazy == 0) {
					pa = pmap_resolve(&map->pmap, currVaddr);
					if (pa != 0U) {
						result = pmap_enter(&map->pmap, pa, currVaddr, (int)attr, NULL);
					}
				}
				else {
					result = _map_force(map, e, currVaddr, (unsigned int)prot);
				}
			}

			lenLeft -= e->size;
			prev = e;
		} while ((lenLeft != 0U) && (result == EOK));
	}

	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)proc_lockClear(&map->lock);

	return result;
}


void vm_mapDump(vm_map_t *map)
{
	if (map == NULL) {
		map = map_common.kmap;
	}

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 973, 975 */
	(void)proc_lockSet(&map->lock);
	lib_rbDump(map->tree.root, map_dump);
	(void)proc_lockClear(&map->lock);
}


int vm_mapCreate(vm_map_t *map, void *start, void *stop)
{
	map->start = start;
	map->stop = stop;
	map->pmap.start = start;
	map->pmap.end = stop;

#ifndef NOMMU
	if ((map->pmap.pmapp = vm_pageAlloc(SIZE_PDIR, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE)) == NULL)
		return -ENOMEM;

	if ((map->pmap.pmapv = vm_mmap(map_common.kmap, NULL, map->pmap.pmapp, 1 << map->pmap.pmapp->idx, PROT_READ | PROT_WRITE, map_common.kernel, -1, MAP_NONE)) == NULL) {
		vm_pageFree(map->pmap.pmapp);
		return -ENOMEM;
	}

	pmap_create(&map->pmap, &map_common.kmap->pmap, map->pmap.pmapp, map->pmap.pmapv);
#else
	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)pmap_create(&map->pmap, &map_common.kmap->pmap, NULL, NULL);
#endif

	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)proc_lockInit(&map->lock, &proc_lockAttrDefault, "map.map");
	lib_rbInit(&map->tree, map_cmp, map_augment);
	return EOK;
}


static void _map_free(map_entry_t *entry)
{
	map_common.nfree++;
	entry->next = map_common.free;
	map_common.free = entry;
}


void map_free(map_entry_t *entry)
{
	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 1019, 1021*/
	(void)proc_lockSet(&map_common.lock);
	_map_free(entry);
	(void)proc_lockClear(&map_common.lock);
}


void vm_mapDestroy(process_t *p, vm_map_t *map)
{
	map_entry_t *e;

#ifndef NOMMU
	addr_t a;
	rbnode_t *n;
	int i = 0;

	while ((a = pmap_destroy(&map->pmap, &i)))
		vm_pageFree(_page_get(a));

	vm_munmap(map_common.kmap, map->pmap.pmapv, SIZE_PDIR);
	vm_pageFree(map->pmap.pmapp);

	for (n = map->tree.root; n != NULL; n = map->tree.root) {
		e = lib_treeof(map_entry_t, linkage, n);
		amap_putanons(e->amap, e->aoffs, e->size);
		_entry_put(map, e);
	}

	proc_lockDone(&map->lock);
#else
	map_entry_t *temp = NULL;

	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)proc_lockSet2(&map->lock, &p->lock);

	while (p->entries != NULL) {
		e = p->entries;
		LIST_REMOVE(&p->entries, e);

		/* Sieve-out entries not belonging to the map at hand */
		if (e->map != map) {
			LIST_ADD(&temp, e);
		}
		else {
			amap_put(e->amap);
			/* MISRA Rule 17.7: Unused returned value, (void) added */
			(void)vm_objectPut(e->object);
			lib_rbRemove(&map->tree, &e->linkage);
			e->map = NULL;
			e->process = NULL;
			map_free(e);
		}
	}

	/* Restore not removed entries */
	while (temp != NULL) {
		e = temp;
		LIST_REMOVE(&temp, e);
		LIST_ADD(&p->entries, e);
	}

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 1080, 1081*/
	(void)proc_lockClear(&p->lock);
	(void)proc_lockClear(&map->lock);
#endif
}


static void remap_readonly(vm_map_t *map, map_entry_t *e, int offs)
{
	addr_t a;
	unsigned int attr = PGHD_PRESENT;

	if ((e->prot & PROT_USER) != 0U) {
		attr |= PGHD_USER;
	}

	if ((a = pmap_resolve(&map->pmap, e->vaddr + offs))) {
		/* MISRA Rule 17.7: Unused returned value, (void) added */
		(void)page_map(&map->pmap, e->vaddr + offs, a, attr);
	}
}


int vm_mapCopy(process_t *proc, vm_map_t *dst, vm_map_t *src)
{
	rbnode_t *n;
	map_entry_t *e, *f;
	int offs;

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 1109, 1120, 1121, 1127, 1143, 1144, 1151, 1152*/
	(void)proc_lockSet2(&src->lock, &dst->lock);

	for (n = lib_rbMinimum(src->tree.root); n != NULL; n = lib_rbNext(n)) {
		e = lib_treeof(map_entry_t, linkage, n);

		if ((e->flags & MAP_NOINHERIT) != 0U) {
			continue;
		}

		f = map_alloc();
		if (f == NULL) {
			(void)proc_lockClear(&dst->lock);
			(void)proc_lockClear(&src->lock);
			vm_mapDestroy(proc, dst);
			return -ENOMEM;
		}

		vm_mapEntryCopy(f, e, 1);
		(void)_map_add(proc, dst, f);

		if (((e->protOrig & PROT_WRITE) != 0U) && ((e->flags & MAP_DEVICE) == 0U)) {
			e->flags |= MAP_NEEDSCOPY;
			f->flags |= MAP_NEEDSCOPY;

			for (offs = 0; offs < (int)f->size; offs += (int)SIZE_PAGE) {
				remap_readonly(src, e, offs);
				remap_readonly(dst, f, offs);
			}
		}

		if ((proc == NULL) || (proc->lazy == 0U)) {
			for (offs = 0; offs < (int)f->size; offs += (int)SIZE_PAGE) {
				if ((_map_force(dst, f, f->vaddr + offs, f->prot) != 0) ||
						(_map_force(src, e, e->vaddr + offs, e->prot) != 0)) {
					(void)proc_lockClear(&dst->lock);
					(void)proc_lockClear(&src->lock);
					return -ENOMEM;
				}
			}
		}
	}

	(void)proc_lockClear(&dst->lock);
	(void)proc_lockClear(&src->lock);

	return EOK;
}


static int _vm_mapBelongs(const struct _process_t *proc, const void *ptr, size_t size)
{
/* Disabled for now on NOMMU, as we can now receive memory
 * from different maps and processes via msg */
#ifndef NOMMU
	map_entry_t e, *f;

	if (size == 0) {
		return 0;
	}

	e.vaddr = (void *)ptr;
	e.size = size;

	f = lib_treeof(map_entry_t, linkage, lib_rbFind(&proc->mapp->tree, &e.linkage));
	if ((f == NULL) && (proc->imapp != NULL)) {
		f = lib_treeof(map_entry_t, linkage, lib_rbFind(&proc->imapp->tree, &e.linkage));
	}

	if (f == NULL) {
		return -1;
	}

#ifdef NOMMU
	if (f->process != proc) {
		return -1;
	}
#endif

#endif

	return 0;
}


int vm_mapBelongs(const struct _process_t *proc, const void *ptr, size_t size)
{
	int ret;

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 1198, 1200 */
	(void)proc_lockSet(&proc->mapp->lock);
	ret = _vm_mapBelongs(proc, ptr, size);
	(void)proc_lockClear(&proc->mapp->lock);

	LIB_ASSERT(ret == 0, "Fault @0x%p (%zu) path: %s, pid: %d\n", ptr, size, proc->path, process_getPid(proc));

	return ret;
}


void vm_mapinfo(meminfo_t *info)
{
	rbnode_t *n;
	map_entry_t *e;
	vm_map_t *map;
	const syspage_map_t *spMap;
	int size;
	process_t *process;
	int i;
	size_t total, free;


	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 1221, 1225, 1236 */
	(void)proc_lockSet(&map_common.lock);
	info->entry.total = map_common.ntotal;
	info->entry.free = map_common.nfree;
	info->entry.sz = sizeof(map_entry_t);
	(void)proc_lockClear(&map_common.lock);

	if (info->entry.mapsz != -1) {
		process = proc_find((int)info->entry.pid);

		if (process == NULL) {
			info->entry.mapsz = -1;
			return;
		}

		if ((map = process->mapp) != NULL) {
			(void)proc_lockSet(&map->lock);

#ifndef NOMMU
			for (size = 0, n = lib_rbMinimum(map->tree.root); n != NULL; n = lib_rbNext(n), ++size) {
				if (info->entry.map != NULL && info->entry.mapsz > size) {
					e = lib_treeof(map_entry_t, linkage, n);

					info->entry.map[size].vaddr = e->vaddr;
					info->entry.map[size].size = e->size;
					info->entry.map[size].flags = e->flags;
					info->entry.map[size].prot = e->prot;
					info->entry.map[size].protOrig = e->protOrig;
					info->entry.map[size].anonsz = ~0;

					if (e->amap != NULL) {
						info->entry.map[size].anonsz = 0;
						for (i = 0; i < e->amap->size; ++i) {
							if (e->amap->anons[i] != NULL)
								info->entry.map[size].anonsz += SIZE_PAGE;
						}
					}

					info->entry.map[size].offs = e->offs;

					if (e->object == NULL) {
						info->entry.map[size].object = OBJECT_ANONYMOUS;
					}
					else if (e->object == VM_OBJ_PHYSMEM) {
						info->entry.map[size].object = OBJECT_MEMORY;
					}
					else {
						info->entry.map[size].object = OBJECT_OID;
						info->entry.map[size].oid = e->object->oid;
					}
				}
			}
#else
			size = 0;
			e = process->entries;

			do {
				if (info->entry.map != NULL && info->entry.mapsz > size) {
					info->entry.map[size].vaddr = e->vaddr;
					info->entry.map[size].size = e->size;
					info->entry.map[size].flags = e->flags;
					info->entry.map[size].prot = e->prot;
					info->entry.map[size].protOrig = e->protOrig;
					info->entry.map[size].anonsz = ~0x0U;

					if (e->amap != NULL) {
						info->entry.map[size].anonsz = 0;
						for (i = 0; i < (int)e->amap->size; ++i) {
							if (e->amap->anons[i] != NULL) {
								info->entry.map[size].anonsz += SIZE_PAGE;
							}
						}
					}

					info->entry.map[size].offs = e->offs;

					if (e->object == NULL) {
						info->entry.map[size].object = OBJECT_ANONYMOUS;
					}
					else if (e->object == VM_OBJ_PHYSMEM) {
						info->entry.map[size].object = OBJECT_MEMORY;
					}
					else {
						info->entry.map[size].object = OBJECT_OID;
						info->entry.map[size].oid = e->object->oid;
					}
				}

				++size;
				e = e->next;
			} while (e != process->entries);
#endif

			/* MISRA Rule 17.7: Unused returned value, (void) added in lines 1314, 1321, 1325 */
			(void)proc_lockClear(&map->lock);
		}
		else {
			size = 0;
		}

		info->entry.mapsz = size;
		(void)proc_put(process);
	}

	if (info->entry.kmapsz != -1) {
		(void)proc_lockSet(&map_common.kmap->lock);

		for (size = 0, n = lib_rbMinimum(map_common.kmap->tree.root); n != NULL; n = lib_rbNext(n), ++size) {
			if (info->entry.kmap != NULL && info->entry.kmapsz > size) {
				e = lib_treeof(map_entry_t, linkage, n);

				info->entry.kmap[size].vaddr = e->vaddr;
				info->entry.kmap[size].size = e->size;
				info->entry.kmap[size].flags = e->flags;
				info->entry.kmap[size].prot = e->prot;
				info->entry.kmap[size].protOrig = e->protOrig;
				info->entry.kmap[size].anonsz = ~0x0U;

				if (e->amap != NULL) {
					info->entry.kmap[size].anonsz = 0x0U;
					for (i = 0; i < (int)e->amap->size; ++i) {
						if (e->amap->anons[i] != NULL) {
							info->entry.kmap[size].anonsz += SIZE_PAGE;
						}
					}
				}

				info->entry.kmap[size].offs = e->offs;

				if (e->object == NULL) {
					info->entry.kmap[size].object = OBJECT_ANONYMOUS;
				}
				else if (e->object == VM_OBJ_PHYSMEM) {
					info->entry.kmap[size].object = OBJECT_MEMORY;
				}
				else {
					info->entry.kmap[size].object = OBJECT_OID;
					info->entry.kmap[size].oid = e->object->oid;
				}
			}
		}

		/* MISRA Rule 17.7: Unused returned value, (void) added */
		(void)proc_lockClear(&map_common.kmap->lock);
		info->entry.kmapsz = size;
	}

	if (info->maps.mapsz != -1) {
		info->maps.total = 0;
		info->maps.free = 0;

		for (size = 0; (unsigned int)size < map_common.mapssz; ++size) {
			map = vm_getSharedMap(size);
			if (map == NULL) {
				if (size < info->maps.mapsz) {
					/* Store info that the map doesn't exist */
					info->maps.map[size].id = size;
					info->maps.map[size].free = 0;
					info->maps.map[size].alloc = 0;
					info->maps.map[size].pstart = 0;
					info->maps.map[size].pend = 0;
					info->maps.map[size].vstart = 0;
					info->maps.map[size].vend = 0;
				}
				continue;
			}

			/* MISRA Rule 11.6: (unsigned int *) added x2 */
			total = (ptr_t)(unsigned int *)map->stop - (ptr_t)(unsigned int *)map->start;
			if (map->tree.root == NULL) {
				free = total; /* Map is empty */
			}
			else {
				e = lib_treeof(map_entry_t, linkage, map->tree.root);
				free = e->lmaxgap + e->rmaxgap;
			}

			/* All maps together */
			info->maps.total += total;
			info->maps.free += free;

			/* MISRA Rule 10.4: (int)size -> size*/
			if (size < info->maps.mapsz) {
				info->maps.map[size].id = size;
				info->maps.map[size].free = free;
				info->maps.map[size].alloc = total - free;
				/* MISRA Rule 11.6: (usigned int *) added in lines 1335-1348*/
				info->maps.map[size].pstart = (addr_t)(unsigned int *)map->pmap.start;
				info->maps.map[size].pend = (addr_t)(unsigned int *)map->pmap.end;
				info->maps.map[size].vstart = (ptr_t)(unsigned int *)map->start;
				info->maps.map[size].vend = (ptr_t)(unsigned int *)map->stop;
				spMap = syspage_mapIdResolve((unsigned int)size);
				if ((spMap != NULL) && (spMap->name != NULL)) {
					/* MISRA Rule 17.7: Unused returned value, (void) added */
					(void)hal_strncpy(info->maps.map[size].name, spMap->name, sizeof(info->maps.map[size].name));
					info->maps.map[size].name[sizeof(info->maps.map[size].name) - 1U] = '\0';
				}
				else {
					info->maps.map[size].name[0] = '\0';
				}
			}
		}

		/* MISRA Rule 10.4: (int)size -> size*/
		info->maps.mapsz = size;
	}
}


/*
 * Entry pool management
 */


static map_entry_t *map_allocN(int n)
{
	map_entry_t *e, *tmp;
	int i;

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 1438, 1441, 1457*/
	(void)proc_lockSet(&map_common.lock);

	if (map_common.nfree < (unsigned int)n) {
		(void)proc_lockClear(&map_common.lock);
#ifndef NDEBUG
		lib_printf("vm: Entry pool exhausted!\n");
#endif
		return NULL;
	}

	map_common.nfree -= (unsigned int)n;
	e = map_common.free;
	tmp = e;
	for (i = 0; i < (n - 1); i++) {
		tmp = tmp->next;
	}
	map_common.free = tmp->next;
	tmp->next = NULL;

	(void)proc_lockClear(&map_common.lock);

	return e;
}


static map_entry_t *map_alloc(void)
{
	return map_allocN(1);
}


void vm_mapGetStats(size_t *allocsz)
{
	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 1472, 1474*/
	(void)proc_lockSet(&map_common.lock);
	*allocsz = (map_common.ntotal - map_common.nfree) * sizeof(map_entry_t);
	(void)proc_lockClear(&map_common.lock);
}


vm_map_t *vm_getSharedMap(int map)
{
	vm_map_t *ret = NULL;

	if (map >= 0 && (size_t)map < map_common.mapssz) {
		ret = map_common.maps[map];
	}

	return ret;
}


static int _map_mapsInit(vm_map_t *kmap, vm_object_t *kernel, void **bss, void **top)
{
#ifdef NOMMU
	size_t mapsCnt, id = 0;
	int result;

	map_entry_t *entry;
	const mapent_t *sysEntry;
	const syspage_map_t *map;

	if ((mapsCnt = syspage_mapSize()) == 0U) {
		return -EINVAL;
	}

	map_common.maps = (vm_map_t **)(*bss);
	while ((*top) - (*bss) < (ptrdiff_t)sizeof(vm_map_t *) * (ptrdiff_t)mapsCnt) {
		result = _page_sbrk(&map_common.kmap->pmap, bss, top);
		LIB_ASSERT_ALWAYS(result >= 0, "vm: Problem with extending kernel heap for vm_map_t pool (vaddr=%p)", *bss);
	}

	(*bss) += (sizeof(vm_map_t *) * mapsCnt);
	map = syspage_mapList();

	do {
		/* MISRA Rule 11.6: Added (unsigned int *) in lines 1451 x2, 1452, 1453, 1455 and 1456*/
		if (kmap->pmap.start >= (void *)(unsigned int *)map->start && kmap->pmap.end <= (void *)(unsigned int *)map->end) {
			kmap->pmap.start = (void *)(unsigned int *)map->start;
			kmap->pmap.end = (void *)(unsigned int *)map->end;

			kmap->start = (void *)(unsigned int *)map->start;
			kmap->stop = (void *)(unsigned int *)map->end;

			map_common.maps[id] = kmap;
		}
		/* Allocate new map */
		else {
			while ((*top) - (*bss) < (ptrdiff_t)sizeof(vm_map_t)) {
				result = _page_sbrk(&map_common.kmap->pmap, bss, top);
				LIB_ASSERT_ALWAYS(result >= 0, "vm: Problem with extending kernel heap for vm_map_t pool (vaddr=%p)", *bss);
			}

			map_common.maps[id] = (*bss);
			/* MISRA Rule 11.6: Added (unsigned int *) x2 */
			if (vm_mapCreate(map_common.maps[id], (void *)(unsigned int *)map->start, (void *)(unsigned int *)map->end) < 0) {
				return -ENOMEM;
			}

			(*bss) += sizeof(vm_map_t);
		}

		if ((sysEntry = map->entries) != NULL) {
			do {
				/* Skip temporary entries which are used only in phoenix-rtos-loader */
				if (sysEntry->type == hal_entryTemp) {
					continue;
				}

				if ((entry = map_alloc()) == NULL) {
					return -ENOMEM;
				}

				entry->vaddr = (void *)round_page((long)sysEntry->start);
				entry->size = round_page(sysEntry->end - sysEntry->start);
				entry->object = kernel;
				entry->offs = -1;
				entry->flags = MAP_NONE;
				/* TODO: initialize map properties based on attributes in syspage */
				entry->prot = PROT_READ | PROT_EXEC;
				entry->protOrig = entry->prot;
				entry->amap = NULL;

				if (_map_add(NULL, map_common.maps[id], entry) < 0) {
					return -ENOMEM;
				}
			} while ((sysEntry = sysEntry->next) != map->entries);
		}

		++id;
	} while ((map = map->next) != syspage_mapList());

	map_common.mapssz = id;
#else
	map_common.maps = NULL;
	map_common.mapssz = 0;
#endif

	return EOK;
}


int _map_init(vm_map_t *kmap, vm_object_t *kernel, void **bss, void **top)
{
	int result, i;
	unsigned int prot;
	void *vaddr;
	size_t poolsz, freesz, size;
	map_entry_t *e;

	/* MISRA Rule 17.7: Unused returned value, (void) added in lines 1589, 1594*/
	(void)proc_lockInit(&map_common.lock, &proc_lockAttrDefault, "map.common");

	kmap->start = kmap->pmap.start;
	kmap->stop = kmap->pmap.end;

	(void)proc_lockInit(&kmap->lock, &proc_lockAttrDefault, "map.kmap");
	lib_rbInit(&kmap->tree, map_cmp, map_augment);

	map_common.kmap = kmap;
	map_common.kernel = kernel;

	vm_pageGetStats(&freesz);

	/* Init map entry pool */
	map_common.ntotal = freesz / (3U * SIZE_PAGE + sizeof(map_entry_t));
	map_common.nfree = map_common.ntotal;

	while ((*top) - (*bss) < (ptrdiff_t)sizeof(map_entry_t) * (ptrdiff_t)map_common.ntotal) {
		result = _page_sbrk(&map_common.kmap->pmap, bss, top);
		LIB_ASSERT_ALWAYS(result >= 0, "vm: Problem with extending kernel heap for map_entry_t pool (vaddr=%p)", *bss);
	}

	map_common.entries = (*bss);
	poolsz = min((*top) - (*bss), sizeof(map_entry_t) * map_common.ntotal);

	map_common.free = map_common.entries;

	for (i = 0; i < (int)map_common.nfree - 1; ++i) {
		map_common.entries[i].next = map_common.entries + i + 1;
	}

	map_common.entries[i].next = NULL;

	(*bss) += poolsz;
	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)lib_printf("vm: Initializing memory mapper: (%d*%d) %d\n", map_common.nfree, sizeof(map_entry_t), poolsz);

	result = _map_mapsInit(kmap, kernel, bss, top);
	LIB_ASSERT_ALWAYS(result >= 0, "vm: Problem with maps initialization.");

	/* Map kernel segments */
	for (i = 0;; i++) {
		prot = PROT_READ | PROT_EXEC;

		if (pmap_segment((unsigned int)i, &vaddr, &size, (int *)&prot, top) < 0) {
			break;
		}

		e = map_alloc();
		if (e == NULL) {
			break;
		}

		e->vaddr = (void *)round_page((long)vaddr);
		e->size = round_page(size);
		e->object = kernel;
		e->offs = -1;
		e->flags = MAP_NONE;
		e->prot = (unsigned char)prot;
		e->protOrig = (unsigned char)prot;
		e->amap = NULL;
		/* MISRA Rule 17.7: Unused returned value, (void) added */
		(void)_map_add(NULL, map_common.kmap, e);
	}


#ifdef EXC_PAGEFAULT
	hal_exceptionsSetHandler(EXC_PAGEFAULT, map_pageFault);
#endif

	return EOK;
}
