/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - amap abstraction
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "include/errno.h"
#include "lib/lib.h"
#include "phmap.h"
#include "proc/proc.h"
#include "amap.h"
#include "map.h"


static struct {
	vm_object_t *kernel;
	vm_map_t *kmap;
} amap_common;


static anon_t *amap_putanon(anon_t *a)
{
	if (a == NULL) {
		return NULL;
	}
	(void)proc_lockSet(&a->lock);
	if (--a->refs != 0) {
		(void)proc_lockClear(&a->lock);
		return a;
	}

	(void)vm_phFree(a->page, SIZE_PAGE);
	(void)proc_lockClear(&a->lock);
	(void)proc_lockDone(&a->lock);
	vm_kfree(a);
	return NULL;
}


void amap_putanons(amap_t *amap, size_t offset, size_t size)
{
	size_t i;

	if (amap == NULL) {
		return;
	}

	(void)proc_lockSet(&amap->lock);
	for (i = offset / SIZE_PAGE; i < (offset + size) / SIZE_PAGE; ++i) {
		(void)amap_putanon(amap->anons[i]);
	}
	(void)proc_lockClear(&amap->lock);
}


static anon_t *amap_getanon(anon_t *a)
{
	if (a == NULL) {
		return NULL;
	}

	(void)proc_lockSet(&a->lock);
	++a->refs;
	(void)proc_lockClear(&a->lock);

	return a;
}


void amap_getanons(amap_t *amap, size_t offset, size_t size)
{
	size_t i;

	if (amap == NULL) {
		return;
	}

	(void)proc_lockSet(&amap->lock);
	for (i = offset / SIZE_PAGE; i < (offset + size) / SIZE_PAGE; ++i) {
		(void)amap_getanon(amap->anons[i]);
	}
	(void)proc_lockClear(&amap->lock);
}


amap_t *amap_ref(amap_t *amap)
{
	if (amap == NULL) {
		return NULL;
	}

	(void)proc_lockSet(&amap->lock);
	amap->refs++;
	(void)proc_lockClear(&amap->lock);

	return amap;
}


amap_t *amap_create(amap_t *amap, size_t *offset, size_t size)
{
	size_t i = size / SIZE_PAGE;
	amap_t *new;

	if (amap != NULL) {
		(void)proc_lockSet(&amap->lock);
		if (amap->refs == 1) {
			(void)proc_lockClear(&amap->lock);
			return amap;
		}

		amap->refs--;
	}

	/* Allocate anon pointer arrays in chunks
	 * to facilitate merging of amaps */
	if (i < (512U - sizeof(amap_t)) / sizeof(anon_t *)) {
		i = (512U - sizeof(amap_t)) / sizeof(anon_t *);
	}

	new = vm_kmalloc(sizeof(amap_t) + i * sizeof(anon_t *));
	if (new == NULL) {
		if (amap != NULL) {
			(void)proc_lockClear(&amap->lock);
		}
		return NULL;
	}

	(void)proc_lockInit(&new->lock, &proc_lockAttrDefault, "amap.map");
	new->size = (unsigned int)i;
	new->refs = 1;
	*offset = *offset / SIZE_PAGE;


	for (i = 0; i < size / SIZE_PAGE; ++i) {
		new->anons[i] = (amap == NULL) ? NULL : amap->anons[*offset + i];
	}

	while (i < new->size) {
		new->anons[i++] = NULL;
	}

	if (amap != NULL) {
		(void)proc_lockClear(&amap->lock);
	}

	*offset = 0;
	return new;
}


void amap_put(amap_t *amap)
{
	if (amap == NULL) {
		return;
	}

	(void)proc_lockSet(&amap->lock);

	if (--amap->refs != 0) {
		(void)proc_lockClear(&amap->lock);
		return;
	}

	(void)proc_lockDone(&amap->lock);
	vm_kfree(amap);
}


void amap_clear(amap_t *amap, size_t offset, size_t size)
{
	size_t i;

	(void)proc_lockSet(&amap->lock);
	for (i = offset / SIZE_PAGE; i < (offset + size) / SIZE_PAGE; i++) {
		amap->anons[i] = NULL;
	}
	(void)proc_lockClear(&amap->lock);
}


static anon_t *anon_new(addr_t p)
{
	anon_t *a;

	a = vm_kmalloc(sizeof(anon_t));
	if (a == NULL) {
		return NULL;
	}

	a->page = p;
	a->refs = 1;
	(void)proc_lockInit(&a->lock, &proc_lockAttrDefault, "amap.anon");

	return a;
}


static void *amap_map(vm_map_t *map, addr_t p)
{
	if (map == amap_common.kmap) {
		return _vm_mmap(amap_common.kmap, NULL, p, SIZE_PAGE, PROT_READ | PROT_WRITE, amap_common.kernel, VM_OFFS_MAX, MAP_NONE);
	}

	return vm_mmap(amap_common.kmap, NULL, p, SIZE_PAGE, PROT_READ | PROT_WRITE, amap_common.kernel, -1, MAP_NONE);
}


static int amap_unmap(vm_map_t *map, void *v)
{
	if (map == amap_common.kmap) {
		return _vm_munmap(amap_common.kmap, v, SIZE_PAGE);
	}

	return vm_munmap(amap_common.kmap, v, SIZE_PAGE);
}


addr_t amap_page(vm_map_t *map, amap_t *amap, vm_object_t *o, void *vaddr, size_t aoffs, u64 offs, vm_prot_t prot)
{
	addr_t p = PHADDR_INVALID;
	anon_t *a;
	void *v, *w;
	size_t size = SIZE_PAGE;

	(void)proc_lockSet(&amap->lock);

	a = amap->anons[aoffs / SIZE_PAGE];
	if (a != NULL) {
		(void)proc_lockSet(&a->lock);
		p = a->page;
		if (!(a->refs > 1 && (prot & PROT_WRITE) != 0U)) {
			(void)proc_lockClear(&a->lock);
			(void)proc_lockClear(&amap->lock);
			return p;
		}
		a->refs--;
	}
	else {
		p = vm_objectPage(map, &amap, o, vaddr, offs);
		if (p == PHADDR_INVALID) {
			/* amap could be invalidated while fetching from the object's store */
			if (amap != NULL) {
				(void)proc_lockClear(&amap->lock);
			}
			return PHADDR_INVALID;
		}
		else if (o != NULL && (prot & PROT_WRITE) == 0U) {
			(void)proc_lockClear(&amap->lock);
			return p;
		}
		else {
			/* No action required */
		}
	}

	v = amap_map(map, p);
	if (v == NULL) {
		if (a != NULL) {
			(void)proc_lockClear(&a->lock);
		}
		(void)proc_lockClear(&amap->lock);
		return PHADDR_INVALID;
	}

	if (a != NULL || o != NULL) {
		/* Copy from object or shared anon */
		p = vm_phAlloc(&size, PAGE_OWNER_APP, MAP_CONTIGUOUS);
		if (p == PHADDR_INVALID) {
			(void)amap_unmap(map, v);
			if (a != NULL) {
				(void)proc_lockClear(&a->lock);
			}
			(void)proc_lockClear(&amap->lock);
			return PHADDR_INVALID;
		}
		w = amap_map(map, p);
		if (w == NULL) {
			(void)vm_phFree(p, size);
			(void)amap_unmap(map, v);
			if (a != NULL) {
				(void)proc_lockClear(&a->lock);
			}
			(void)proc_lockClear(&amap->lock);
			return PHADDR_INVALID;
		}
		hal_memcpy(w, v, SIZE_PAGE);
		(void)amap_unmap(map, w);
	}
	else {
		hal_memset(v, 0, SIZE_PAGE);
	}

	(void)amap_unmap(map, v);

	if (a != NULL) {
		(void)proc_lockClear(&a->lock);
	}

	amap->anons[aoffs / SIZE_PAGE] = anon_new(p);
	if (amap->anons[aoffs / SIZE_PAGE] == NULL) {
		(void)vm_phFree(p, size);
		p = PHADDR_INVALID;
	}
	(void)proc_lockClear(&amap->lock);

	return p;
}


void _amap_init(vm_map_t *kmap, vm_object_t *kernel)
{
	amap_common.kmap = kmap;
	amap_common.kernel = kernel;
}
