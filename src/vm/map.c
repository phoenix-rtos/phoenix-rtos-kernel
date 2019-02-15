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

#include HAL
#include "../../include/errno.h"
#include "../../include/signal.h"
#include "../lib/lib.h"
#include "map.h"
#include "../proc/proc.h"
#include "amap.h"


extern void _etext(void);


struct {
	vm_map_t *kmap;
	vm_object_t *kernel;

	lock_t lock;

	unsigned int ntotal, nfree;
	map_entry_t *free;
	map_entry_t *entries;
} map_common;


map_entry_t *map_alloc(void);


void map_free(map_entry_t *entry);


static int _map_force(vm_map_t *map, map_entry_t *e, void *paddr, int prot);


static int map_cmp(rbnode_t *n1, rbnode_t *n2)
{
	map_entry_t *e1 = lib_treeof(map_entry_t, linkage, n1);
	map_entry_t *e2 = lib_treeof(map_entry_t, linkage, n2);

	if (e2->vaddr + e2->size <= e1->vaddr)
		return 1;

	if (e1->vaddr + e1->size <= e2->vaddr)
		return -1;

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
			if (it->parent->right == it)
				break;
		}

		n->lmaxgap = (size_t) (n->vaddr <= p->vaddr) ? (n->vaddr - n->map->start) : (n->vaddr - p->vaddr) - p->size;
	}
	else {
		map_entry_t *l = lib_treeof(map_entry_t, linkage, node->left);
		n->lmaxgap = max(l->lmaxgap, l->rmaxgap);
	}

	if (node->right == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(map_entry_t, linkage, it->parent);
			if (it->parent->left == it)
				break;
		}

		n->rmaxgap = (size_t) (n->vaddr >= p->vaddr) ? (n->map->stop - n->vaddr) - n->size : (p->vaddr - n->vaddr) - n->size;
	}
	else {
		map_entry_t *r = lib_treeof(map_entry_t, linkage, node->right);
		n->rmaxgap = max(r->lmaxgap, r->rmaxgap);
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(map_entry_t, linkage, it);
		p = lib_treeof(map_entry_t, linkage, it->parent);

		if (it->parent->left == it)
			p->lmaxgap = max(n->lmaxgap, n->rmaxgap);
		else
			p->rmaxgap = max(n->lmaxgap, n->rmaxgap);
	}
}


void map_dump(rbnode_t *node)
{
	map_entry_t *e = lib_treeof(map_entry_t, linkage, node);
	lib_printf("%p+%x, %x, %x", e->vaddr, e->size, e->lmaxgap, e->rmaxgap);
}


static int _map_add(process_t *p, vm_map_t *map, map_entry_t *entry)
{
#ifdef NOMMU
	if (p != NULL) {
		proc_lockSet(&p->lock);
		LIST_ADD(&p->entries, entry);
		proc_lockClear(&p->lock);
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
		proc_lockSet(&p->lock);
		LIST_REMOVE(&p->entries, entry);
		proc_lockClear(&p->lock);
	}
	entry->process = NULL;
#endif

	lib_rbRemove(&map->tree, &entry->linkage);
	entry->map = NULL;
}


static void _entry_put(vm_map_t *map, map_entry_t *e)
{
	amap_put(e->amap);
	vm_objectPut(e->object);
	_map_remove(map, e);
	map_free(e);
}


void *_map_find(vm_map_t *map, void *vaddr, size_t size, map_entry_t **prev, map_entry_t **next)
{
	map_entry_t *e = lib_treeof(map_entry_t, linkage, map->tree.root);

	*prev = NULL;
	*next = NULL;

	if (((void *)map->stop - size) < vaddr)
 		return NULL;

	if (vaddr < map->start)
		vaddr = map->start;

	while (e != NULL) {

		if ((size <= e->lmaxgap) && ((vaddr + size) <= e->vaddr)) {
			*next = e;

			if (e->linkage.left == NULL)
				return max(vaddr, e->vaddr - e->lmaxgap);

			e = lib_treeof(map_entry_t, linkage, e->linkage.left);
			continue;
		}

		if ((size <= e->rmaxgap) /*&& (vaddr + size) <= (e->vaddr + e->size + e->rmaxgap)*/) {
			*prev = e;

			if (e->linkage.right == NULL)
				return max(vaddr, e->vaddr + e->size);

			e = lib_treeof(map_entry_t, linkage, e->linkage.right);
			continue;
		}

		for (;; e = lib_treeof(map_entry_t, linkage, e->linkage.parent)) {
			if (e->linkage.parent == NULL)
				return NULL;

			if ((e == lib_treeof(map_entry_t, linkage, e->linkage.parent->left)) && ((lib_treeof(map_entry_t, linkage, e->linkage.parent)->rmaxgap >= size)))
				break;
		}
		e = lib_treeof(map_entry_t, linkage, e->linkage.parent);

		for (*next = e; (*next)->linkage.parent != NULL; *next = lib_treeof(map_entry_t, linkage, (*next)->linkage.parent))
			if ((*next) == lib_treeof(map_entry_t, linkage, (*next)->linkage.parent->left))
				break;

		*next = lib_treeof(map_entry_t, linkage, (*next)->linkage.parent);

		*prev = e;
		if (e->linkage.right == NULL)
			return e->vaddr + e->size;

		e = lib_treeof(map_entry_t, linkage, e->linkage.right);
	}

	return vaddr;
}


static void *_map_map(vm_map_t *map, void *vaddr, process_t *proc, size_t size, u8 prot, vm_object_t *o, offs_t offs, u8 flags, map_entry_t **entry)
{
	void *v;
	map_entry_t *prev, *next, *e;
	unsigned int lmerge, rmerge;
	amap_t *amap;

	if ((v = _map_find(map, vaddr, size, &prev, &next)) == NULL)
		return NULL;

	rmerge = next != NULL && v + size == next->vaddr && next->object == o && next->flags == flags && next->prot == prot;
	lmerge = prev != NULL && v == prev->vaddr + prev->size && prev->object == o && prev->flags == flags && prev->prot == prot;

	if (offs != -1) {
		if (offs & (SIZE_PAGE - 1))
			return NULL;

		if (rmerge)
			rmerge &= next->offs == offs + size;

		if (lmerge)
			lmerge &= offs == prev->offs + prev->size;
	}

#ifdef NOMMU
	rmerge = rmerge && proc == next->process;
	lmerge = lmerge && proc == prev->process;
#endif

#if 1
	if (o == NULL) {
		if (lmerge && rmerge && (next->amap == prev->amap)) {
			/* Both use the same amap, can merge */
		}
		else {
			/* Can't merge to the left if amap array size is too small */
			if (lmerge && (amap = prev->amap) != NULL && amap->size * SIZE_PAGE - prev->aoffs - prev->size < size)
				lmerge = 0;

			/* Can't merge to the right if amap offset is too small */
			if (rmerge && (amap = next->amap) != NULL && next->aoffs < size)
				rmerge = 0;

			/* amaps differ, we can only merge one way */
			if (lmerge && rmerge)
				rmerge = 0;
		}
	}
#else
	/* Disable merging of anonymous entries */
	if (o == NULL)
		rmerge = lmerge = 0;
#endif

	if (rmerge && lmerge) {
		e = prev;
		e->size += size + next->size;
		e->rmaxgap = next->rmaxgap;

		map_augment(&e->linkage);
		_entry_put(map, next);
	}
	else if (rmerge) {
		e = next;
		e->vaddr = v;
		e->offs = offs;
		e->size += size;
		e->lmaxgap -= size;

		if (e->aoffs)
			e->aoffs -= size;

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
		if ((e = map_alloc()) == NULL)
			return NULL;

		e->vaddr = v;
		e->size = size;
		e->object = vm_objectRef(o);
		e->offs = offs;
		e->flags = flags;
		e->prot = prot;

		e->amap = NULL;
		e->aoffs = 0;

		if (o == NULL) {
			/* Try to use existing amap */
			if (next != NULL && next->amap != NULL && next->aoffs >= (next->vaddr - e->vaddr)) {
				e->amap = amap_ref(next->amap);
				e->aoffs = next->aoffs - (next->vaddr - e->vaddr);
			}
			else if (prev != NULL && prev->amap != NULL && SIZE_PAGE * prev->amap->size - prev->aoffs + prev->vaddr >= e->vaddr + size) {
				e->amap = amap_ref(prev->amap);
				e->aoffs = prev->aoffs + (e->vaddr - prev->vaddr);
			}
		}

		_map_add(proc, map, e);
	}

	if (entry != NULL)
		*entry = e;

	return v;
}


void *vm_mapFind(vm_map_t *map, void *vaddr, size_t size, u8 flags, u8 prot)
{
	proc_lockSet(&map->lock);
	vaddr = _map_map(map, vaddr, NULL, size, prot, map_common.kernel, -1, flags, NULL);
	proc_lockClear(&map->lock);

	return vaddr;
}


int _vm_munmap(vm_map_t *map, void *vaddr, size_t size)
{
	int offs;
	map_entry_t *e, *s;
	map_entry_t t;
	process_t *proc = proc_current()->process;

	t.vaddr = vaddr;
	t.size = size;

	e = lib_treeof(map_entry_t, linkage, lib_rbFind(&map->tree, &t.linkage));

	if (e == NULL)
		return -EINVAL;

	amap_putanons(e->amap, e->aoffs + vaddr - e->vaddr, size);

	for (offs = vaddr - e->vaddr; offs < vaddr + size - e->vaddr; offs += SIZE_PAGE)
		pmap_remove(&map->pmap, e->vaddr + offs);

	if (e->vaddr == vaddr) {
		if (e->size == size) {
			_entry_put(map, e);
		}
		else {
			e->aoffs += size;
			e->vaddr += size;
			e->size -= size;
			e->lmaxgap += size;

			if ((s = lib_treeof(map_entry_t, linkage, lib_rbPrev(&e->linkage))) != NULL) {
				s->rmaxgap += size;
				map_augment(&s->linkage);
			}

			map_augment(&e->linkage);
		}
	}
	else if (e->vaddr + e->size == vaddr + size) {
		e->size -= size;
		e->rmaxgap += size;

		if ((s = lib_treeof(map_entry_t, linkage, lib_rbNext(&e->linkage))) != NULL) {
			s->lmaxgap += size;
			map_augment(&s->linkage);
		}

		map_augment(&e->linkage);
	}
	else {
		if ((s = map_alloc()) == NULL)
			return -ENOMEM;

		s->flags = e->flags;
		s->prot = e->prot;
		s->object = vm_objectRef(e->object);
		s->offs = (e->offs == -1) ? -1 : e->offs + (vaddr + size - e->vaddr);
		s->vaddr = vaddr + size;
		s->size = (size_t) (e->vaddr + e->size - s->vaddr);
		s->aoffs = e->aoffs + (vaddr + size - e->vaddr);

		s->amap = amap_ref(e->amap);

		e->size = (size_t) (vaddr - e->vaddr);
		e->rmaxgap = size;

		map_augment(&e->linkage);
		_map_add(proc, map, s);
	}

	return EOK;
}


void *_vm_mmap(vm_map_t *map, void *vaddr, page_t *p, size_t size, u8 prot, vm_object_t *o, offs_t offs, u8 flags)
{
	int attr = PROT_NONE;
	void *w;
	process_t *process = NULL;
	thread_t *current;
	map_entry_t *e;

	if (!size || (size & (SIZE_PAGE - 1)))
		return NULL;

	/* NULL page indicates that proc sybsystem is ready */
	if (p == NULL && (current = proc_current()) != NULL)
		process = current->process;
	else if (p != NULL && p->idx != 0)
		size = 1 << p->idx;

	if ((vaddr = _map_map(map, vaddr, process, size, prot, o, offs, flags, &e)) == NULL)
		return NULL;

	if (p != NULL) {
		if (prot & PROT_USER)
			attr |= PGHD_USER;

		if (prot & PROT_WRITE)
			attr |= PGHD_WRITE | PGHD_PRESENT;

		if (prot & PROT_READ)
			attr |= PGHD_PRESENT;

		if (prot & PROT_EXEC)
			attr |= PGHD_EXEC;

		if (flags & MAP_UNCACHED)
			attr |= PGHD_NOT_CACHED;

		if (flags & MAP_DEVICE)
			attr |= PGHD_DEV;

		for (w = vaddr; w < vaddr + size; w += SIZE_PAGE)
			page_map(&map->pmap, w, (p++)->addr, attr);

		return vaddr;
	}

	if (process != NULL && process->lazy)
		return vaddr;

	for (w = vaddr; w < vaddr + size; w += SIZE_PAGE) {
		if (_map_force(map, e, w, prot)) {
			amap_putanons(e->amap, e->aoffs, w - vaddr);

			do
				pmap_remove(&map->pmap, w);
			while (w > vaddr && (w -= SIZE_PAGE));

			_entry_put(map, e);
			return NULL;
		}
	}

	return vaddr;
}


void *vm_mmap(vm_map_t *map, void *vaddr, page_t *p, size_t size, u8 prot, vm_object_t *o, offs_t offs, u8 flags)
{
	if (map == NULL)
		map = map_common.kmap;

	proc_lockSet(&map->lock);
	vaddr = _vm_mmap(map, vaddr, p, size, prot, o, offs, flags);
	proc_lockClear(&map->lock);
	return vaddr;
}


/*
 * Fault routines
 */

int vm_lockVerify(vm_map_t *map, amap_t **amap, vm_object_t *o, void *vaddr, offs_t offs)
{
	map_entry_t t, *e;

	proc_lockSet(&map->lock);

	t.vaddr = vaddr;
	t.size = SIZE_PAGE;

	e = lib_treeof(map_entry_t, linkage, lib_rbFind(&map->tree, &t.linkage));

	if (e == NULL || e->object != o || (amap != NULL && e->amap != *amap) /* More checks? */) {
		if (amap != NULL)
			*amap = NULL;

		return -EINVAL;
	}

	if (amap != NULL)
		proc_lockSet(&(*amap)->lock);

	return EOK;
}


int vm_mapFlags(vm_map_t *map, void *vaddr)
{
	int flags;
	map_entry_t t, *e;

	proc_lockSet(&map->lock);

	t.vaddr = vaddr;
	t.size = SIZE_PAGE;

	e = lib_treeof(map_entry_t, linkage, lib_rbFind(&map->tree, &t.linkage));

	if (e == NULL) {
		proc_lockClear(&map->lock);
		return -EFAULT;
	}

	flags = e->flags & ~MAP_NEEDSCOPY;
	proc_lockClear(&map->lock);

	return flags;
}


int vm_mapForce(vm_map_t *map, void *paddr, int prot)
{
	map_entry_t t, *e;
	int err;

	proc_lockSet(&map->lock);

	t.vaddr = paddr;
	t.size = SIZE_PAGE;

	e = lib_treeof(map_entry_t, linkage, lib_rbFind(&map->tree, &t.linkage));

	if (e == NULL) {
		proc_lockClear(&map->lock);
		return -EFAULT;
	}

	err = _map_force(map, e, paddr, prot);
	proc_lockClear(&map->lock);
	return err;
}


static int _map_force(vm_map_t *map, map_entry_t *e, void *paddr, int prot)
{
	int attr = 0, offs;
	page_t *p;

	if (prot & PROT_WRITE && !(e->prot & PROT_WRITE))
		return PROT_WRITE;

	if (prot & PROT_READ && !(e->prot & PROT_READ))
		return PROT_READ;

	if (prot & PROT_USER && !(e->prot & PROT_USER))
		return PROT_USER;

	if (prot & PROT_EXEC && !(e->prot & PROT_EXEC))
		return PROT_EXEC;

	if ((prot & PROT_WRITE && e->flags & MAP_NEEDSCOPY) || (e->object == NULL && e->amap == NULL)) {
		if ((e->amap = amap_create(e->amap, &e->aoffs, e->size)) == NULL)
			return -ENOMEM;

		e->flags &= ~MAP_NEEDSCOPY;
	}

	offs = paddr - e->vaddr;

	if (e->amap == NULL)
		p = vm_objectPage(map, NULL, e->object, paddr, (e->offs < 0) ? e->offs : e->offs + offs);
	else
		p = amap_page(map, e->amap, e->object, paddr, e->aoffs + offs, (e->offs < 0) ? e->offs : e->offs + offs, prot);

	if (prot & PROT_WRITE)
		attr |= PGHD_WRITE | PGHD_PRESENT;

	if (prot & PROT_READ)
		attr |= PGHD_PRESENT;

	if (prot & PROT_USER)
		attr |= PGHD_USER;

	if (prot & PROT_EXEC)
		attr |= PGHD_EXEC;

	if (e->flags & MAP_UNCACHED)
		attr |= PGHD_NOT_CACHED;

	if (e->flags & MAP_DEVICE)
		attr |= PGHD_DEV;

	if (p == NULL && e->object == (void *)-1) {
		if (page_map(&map->pmap, paddr, e->offs + offs, attr) < 0)
			return -ENOMEM;
	}
	else if (p == NULL) {
		return -ENOMEM;
	}
	else if (page_map(&map->pmap, paddr, p->addr, attr) < 0) {
		amap_putanons(e->amap, e->aoffs + offs, SIZE_PAGE);
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

		proc_sigpost(thread->process, thread, signal_segv);
	}
}
#endif


int vm_munmap(vm_map_t *map, void *vaddr, size_t size)
{
	int result;

	proc_lockSet(&map->lock);
	result = _vm_munmap(map, vaddr, size);
	proc_lockClear(&map->lock);

	return result;
}


void vm_mapDump(vm_map_t *map)
{
	if (map == NULL)
		map = map_common.kmap;

	proc_lockSet(&map->lock);
	lib_rbDump(map->tree.root, map_dump);
	proc_lockClear(&map->lock);
}


int vm_mapCreate(vm_map_t *map, void *start, void *stop)
{
	map->start = start;
	map->stop = stop;
	map->pmap.start = start;
	map->pmap.end = stop;

	proc_lockInit(&map->lock);
	lib_rbInit(&map->tree, map_cmp, map_augment);
	return EOK;
}


void vm_mapDestroy(process_t *p, vm_map_t *map)
{
	map_entry_t *e;

#ifdef NOMMU
	proc_lockSet(&map->lock);
	while ((e = p->entries) != NULL) {
		_map_remove(map, e);
		map_free(e);
	}
	proc_lockClear(&map->lock);
#else
	rbnode_t *n;

	proc_lockSet(&map->lock);
	for (n = map->tree.root; n != NULL; n = map->tree.root) {
		e = lib_treeof(map_entry_t, linkage, n);
		amap_putanons(e->amap, e->aoffs, e->size);
		_entry_put(map, e);
	}
	proc_lockClear(&map->lock);
	proc_lockDone(&map->lock);
#endif
}


static void remap_readonly(vm_map_t *map, map_entry_t *e, int offs)
{
	addr_t a;
	int attr = PGHD_PRESENT;

	if (e->prot & PROT_USER)
		attr |= PGHD_USER;

	if ((a = pmap_resolve(&map->pmap, e->vaddr + offs)))
		page_map(&map->pmap, e->vaddr + offs, a, attr);
}


int vm_mapCopy(process_t *proc, vm_map_t *dst, vm_map_t *src)
{
	rbnode_t *n;
	map_entry_t *e, *f;
	int offs;

	proc_lockSet2(&src->lock, &dst->lock);

	for (n = lib_rbMinimum(src->tree.root); n != NULL; n = lib_rbNext(n)) {
		e = lib_treeof(map_entry_t, linkage, n);

		if (e->flags & MAP_NOINHERIT)
			continue;

		if ((f = map_alloc()) == NULL) {
			proc_lockClear(&dst->lock);
			proc_lockClear(&src->lock);
			vm_mapDestroy(proc, dst);
			return -ENOMEM;
		}

		hal_memcpy(f, e, sizeof(map_entry_t));
		f->amap = amap_ref(e->amap);
		amap_getanons(f->amap, f->aoffs, f->size);
		f->object = vm_objectRef(e->object);
		_map_add(proc, dst, f);

		if ((e->prot & PROT_WRITE) && !(e->flags & MAP_DEVICE)) {
			e->flags |= MAP_NEEDSCOPY;
			f->flags |= MAP_NEEDSCOPY;

			for (offs = 0; offs < f->size; offs += SIZE_PAGE) {
				remap_readonly(src, e, offs);
				remap_readonly(dst, f, offs);
			}
		}

		if (proc == NULL || !proc->lazy) {
			for (offs = 0; offs < f->size; offs += SIZE_PAGE) {
				if (_map_force(dst, f, f->vaddr + offs, f->prot) < 0 ||
				    _map_force(src, e, e->vaddr + offs, e->prot) < 0) {
					proc_lockClear(&dst->lock);
					proc_lockClear(&src->lock);
					return -ENOMEM;
				}
			}
		}
	}

	proc_lockClear(&dst->lock);
	proc_lockClear(&src->lock);

	return EOK;
}


void vm_mapMove(vm_map_t *dst, vm_map_t *src)
{
	rbnode_t *n;
	map_entry_t *e;

	proc_lockSet(&src->lock);
	proc_lockDone(&src->lock);
	hal_memcpy(dst, src, sizeof(vm_map_t));
	pmap_moved(&dst->pmap);
	proc_lockInit(&dst->lock);
	proc_lockSet(&dst->lock);

	for (n = lib_rbMinimum(src->tree.root); n != NULL; n = lib_rbNext(n)) {
		e = lib_treeof(map_entry_t, linkage, n);
		e->map = dst;
	}

	proc_lockClear(&dst->lock);
}


void vm_mapinfo(meminfo_t *info)
{
	rbnode_t *n;
	map_entry_t *e;
	vm_map_t *map;
	unsigned int size;
	process_t *process;
	int i;


	proc_lockSet(&map_common.lock);
	info->entry.total = map_common.ntotal;
	info->entry.free = map_common.nfree;
	info->entry.sz = sizeof(map_entry_t);
	proc_lockClear(&map_common.lock);

	if (info->entry.mapsz != -1) {
		process = proc_find(info->entry.pid);

		if (process == NULL) {
			info->entry.mapsz = -1;
			proc_lockClear(&map_common.lock);
			return;
		}

		map = process->mapp;
		proc_lockSet(&map->lock);

#ifndef NOMMU
		for (size = 0, n = lib_rbMinimum(map->tree.root); n != NULL; n = lib_rbNext(n), ++size) {
			if (info->entry.map != NULL && info->entry.mapsz > size) {
				e = lib_treeof(map_entry_t, linkage, n);

				info->entry.map[size].vaddr  = e->vaddr;
				info->entry.map[size].size   = e->size;
				info->entry.map[size].flags  = e->flags;
				info->entry.map[size].prot   = e->prot;
				info->entry.map[size].anonsz = ~0;

				if (e->amap != NULL) {
					info->entry.map[size].anonsz = 0;
					for (i = 0; i < e->amap->size; ++i) {
						if (e->amap->anons[i] != NULL)
							info->entry.map[size].anonsz += SIZE_PAGE;
					}
				}

				info->entry.map[size].offs   = e->offs;

				if (e->object == NULL)
					info->entry.map[size].object = OBJECT_ANONYMOUS;

				else if (e->object == (void *)-1)
					info->entry.map[size].object = OBJECT_MEMORY;

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
				info->entry.map[size].size  = e->size;
				info->entry.map[size].flags = e->flags;
				info->entry.map[size].prot  = e->prot;
				info->entry.map[size].anonsz = ~0;

				if (e->amap != NULL) {
					info->entry.map[size].anonsz = 0;
					for (i = 0; i < e->amap->size; ++i) {
						if (e->amap->anons[i] != NULL)
							info->entry.map[size].anonsz += SIZE_PAGE;
					}
				}

				info->entry.map[size].offs  = e->offs;

				if (e->object == NULL)
					info->entry.map[size].object = OBJECT_ANONYMOUS;

				else if (e->object == (void *)-1)
					info->entry.map[size].object = OBJECT_MEMORY;

				else {
					info->entry.map[size].object = OBJECT_OID;
					info->entry.map[size].oid = e->object->oid;
				}
			}

			++size;
			e = e->next;
		}
		while (e != process->entries);
#endif

		proc_lockClear(&map->lock);
		info->entry.mapsz = size;
	}

	if (info->entry.kmapsz != -1) {
		proc_lockSet(&map_common.kmap->lock);

		for (size = 0, n = lib_rbMinimum(map_common.kmap->tree.root); n != NULL; n = lib_rbNext(n), ++size) {
			if (info->entry.kmap != NULL && info->entry.kmapsz > size) {
				e = lib_treeof(map_entry_t, linkage, n);

				info->entry.kmap[size].vaddr = e->vaddr;
				info->entry.kmap[size].size  = e->size;
				info->entry.kmap[size].flags = e->flags;
				info->entry.kmap[size].prot  = e->prot;
				info->entry.kmap[size].anonsz = ~0;

				if (e->amap != NULL) {
					info->entry.kmap[size].anonsz = 0;
					for (i = 0; i < e->amap->size; ++i) {
						if (e->amap->anons[i] != NULL)
							info->entry.kmap[size].anonsz += SIZE_PAGE;
					}
				}

				info->entry.kmap[size].offs   = e->offs;

				if (e->object == NULL)
					info->entry.kmap[size].object = OBJECT_ANONYMOUS;

				else if (e->object == (void *)-1)
					info->entry.kmap[size].object = OBJECT_MEMORY;

				else {
					info->entry.kmap[size].object = OBJECT_OID;
					info->entry.kmap[size].oid = e->object->oid;
				}
			}
		}

		proc_lockClear(&map_common.kmap->lock);
		info->entry.kmapsz = size;
	}
}


/*
 * Entry pool management
 */

map_entry_t *map_alloc(void)
{
	map_entry_t *e;

	proc_lockSet(&map_common.lock);

	if (!map_common.nfree) {
#ifndef NDEBUG
		lib_printf("vm: Entry pool exhausted!\n");
#endif
		proc_lockClear(&map_common.lock);
		return NULL;
	}

	map_common.nfree--;
	e = map_common.free;
	map_common.free = e->next;

	proc_lockClear(&map_common.lock);

	return e;
}


void map_free(map_entry_t *entry)
{
	proc_lockSet(&map_common.lock);
	map_common.nfree++;
	entry->next = map_common.free;
	map_common.free = entry;
	proc_lockClear(&map_common.lock);
}


void vm_mapGetStats(size_t *allocsz)
{
	proc_lockSet(&map_common.lock);
	*allocsz = (map_common.ntotal - map_common.nfree) * sizeof(map_entry_t);
	proc_lockClear(&map_common.lock);
}


int _map_init(vm_map_t *kmap, vm_object_t *kernel, void **bss, void **top)
{
	int i, prot;
	size_t poolsz, freesz, size;
	map_entry_t *e;
	void *vaddr;

	proc_lockInit(&map_common.lock);

	vm_mapCreate(kmap, (void *)VADDR_KERNEL, kmap->pmap.end);
	map_common.kmap = kmap;
	map_common.kernel = kernel;

	vm_pageGetStats(&freesz);

	/* Init map entry pool */
	map_common.nfree = map_common.ntotal = freesz / (4 * SIZE_PAGE + sizeof(map_entry_t));

	while ((*top) - (*bss) < sizeof(map_entry_t) * map_common.ntotal)
		_page_sbrk(&map_common.kmap->pmap, bss, top);

	map_common.entries = (*bss);
	poolsz = min((*top) - (*bss), sizeof(map_entry_t) * map_common.ntotal);

	map_common.free = map_common.entries;

	for (i = 0; i < map_common.nfree - 1; ++i)
		map_common.entries[i].next = map_common.entries + i + 1;

	map_common.entries[i].next = NULL;

	(*bss) += poolsz;

	lib_printf("vm: Initializing memory mapper: (%d*%d) %d\n", map_common.nfree, sizeof(map_entry_t), poolsz);

	/* Map kernel segments */
	for (i = 0;; i++) {
		prot = PROT_READ | PROT_EXEC;

		if (pmap_segment(i, &vaddr, &size, &prot, top) < 0)
			break;

		e = map_alloc();
		e->vaddr = (void *)round_page((long)vaddr);
		e->size = round_page(size);
		e->object = kernel;
		e->offs = -1;
		e->flags = MAP_NONE;
		e->prot = prot;
		e->amap = NULL;
		_map_add(NULL, kmap, e);
	}

#ifdef EXC_PAGEFAULT
	hal_exceptionsSetHandler(EXC_PAGEFAULT, map_pageFault);
#endif

	return EOK;
}
