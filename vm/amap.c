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
#include "page.h"
#include "proc/proc.h"
#include "amap.h"
#include "map.h"


struct {
	vm_object_t *kernel;
	vm_map_t *kmap;
} amap_common;


static anon_t *amap_putanon(anon_t *a)
{
	if (a == NULL)
		return NULL;

	proc_lockSet(&a->lock);
	if (--a->refs) {
		proc_lockClear(&a->lock);
		return a;
	}

	vm_pageFree(a->page);
	proc_lockClear(&a->lock);
	proc_lockDone(&a->lock);
	vm_kfree(a);
	return NULL;
}


void amap_putanons(amap_t *amap, int offset, int size)
{
	int i;

	if (amap == NULL)
		return;

	proc_lockSet(&amap->lock);
	for (i = offset / SIZE_PAGE; i < (offset + size) / SIZE_PAGE; ++i)
		amap_putanon(amap->anons[i]);
	proc_lockClear(&amap->lock);
}


static anon_t *amap_getanon(anon_t *a)
{
	if (a == NULL)
		return NULL;

	proc_lockSet(&a->lock);
	++a->refs;
	proc_lockClear(&a->lock);

	return a;
}


void amap_getanons(amap_t *amap, int offset, int size)
{
	int i;

	if (amap == NULL)
		return;

	proc_lockSet(&amap->lock);
	for (i = offset / SIZE_PAGE; i < (offset + size) / SIZE_PAGE; ++i)
		amap_getanon(amap->anons[i]);
	proc_lockClear(&amap->lock);
}


amap_t *amap_ref(amap_t *amap)
{
	if (amap == NULL)
		return NULL;

	proc_lockSet(&amap->lock);
	amap->refs++;
	proc_lockClear(&amap->lock);

	return amap;
}


amap_t *amap_create(amap_t *amap, int *offset, size_t size)
{
	int i = size / SIZE_PAGE;
	amap_t *new;

	if (amap != NULL) {
		proc_lockSet(&amap->lock);
		if (amap->refs == 1) {
			proc_lockClear(&amap->lock);
			return amap;
		}

		amap->refs--;
	}

	/* Allocate anon pointer arrays in chunks
	 * to facilitate merging of amaps */
	if (i < (512 - sizeof(amap_t)) / sizeof(anon_t *))
		i = (512 - sizeof(amap_t)) / sizeof(anon_t *);

	if ((new = vm_kmalloc(sizeof(amap_t) + i * sizeof(anon_t *))) == NULL) {
		if (amap != NULL)
			proc_lockClear(&amap->lock);

		return NULL;
	}

	proc_lockInit(&new->lock, "amap.map");
	new->size = i;
	new->refs = 1;
	*offset = *offset / SIZE_PAGE;

	for (i = 0; i < size / SIZE_PAGE; ++i)
		new->anons[i] = (amap == NULL) ? NULL : amap->anons[*offset + i];

	while (i < new->size)
		new->anons[i++] = NULL;

	if (amap != NULL)
		proc_lockClear(&amap->lock);

	*offset = 0;
	return new;
}


void amap_put(amap_t *amap)
{
	if (amap == NULL)
		return;

	proc_lockSet(&amap->lock);

	if (--amap->refs) {
		proc_lockClear(&amap->lock);
		return;
	}

	proc_lockDone(&amap->lock);
	vm_kfree(amap);
}


void amap_clear(amap_t *amap, size_t offset, size_t size)
{
	int i;

	proc_lockSet(&amap->lock);
	for (i = offset / SIZE_PAGE; i < (offset + size) / SIZE_PAGE; i++)
		amap->anons[i] = NULL;
	proc_lockClear(&amap->lock);
}


static anon_t *anon_new(page_t *p)
{
	anon_t *a;

	if ((a = vm_kmalloc(sizeof(anon_t))) == NULL)
		return NULL;

	a->page = p;
	a->refs = 1;
	proc_lockInit(&a->lock, "amap.anon");

	return a;
}


static void *amap_map(vm_map_t *map, page_t *p)
{
	if (map == amap_common.kmap)
		return _vm_mmap(amap_common.kmap, NULL, p, SIZE_PAGE, PROT_READ | PROT_WRITE, amap_common.kernel, -1, MAP_NONE);

	return vm_mmap(amap_common.kmap, NULL, p, SIZE_PAGE, PROT_READ | PROT_WRITE, amap_common.kernel, -1, MAP_NONE);
}


static int amap_unmap(vm_map_t *map, void *v)
{
	if (map == amap_common.kmap)
		return _vm_munmap(amap_common.kmap, v, SIZE_PAGE);

	return vm_munmap(amap_common.kmap, v, SIZE_PAGE);
}


page_t *amap_page(vm_map_t *map, amap_t *amap, vm_object_t *o, void *vaddr, int aoffs, off_t offs, int prot)
{
	page_t *p = NULL;
	anon_t *a;
	void *v, *w;

	proc_lockSet(&amap->lock);

	if ((a = amap->anons[aoffs / SIZE_PAGE]) != NULL) {
		proc_lockSet(&a->lock);
		p = a->page;
		if (!(a->refs > 1 && (prot & PROT_WRITE))) {
			proc_lockClear(&a->lock);
			proc_lockClear(&amap->lock);
			return p;
		}
		a->refs--;
	}
	else if ((p = vm_objectPage(map, &amap, o, vaddr, offs)) == NULL) {
		/* amap could be invalidated while fetching from the object's store */
		if (amap != NULL)
			proc_lockClear(&amap->lock);

		return NULL;
	}
	else if (o != NULL && !(prot & PROT_WRITE)) {
		proc_lockClear(&amap->lock);
		return p;
	}

	if ((v = amap_map(map, p)) == NULL) {
		if (a != NULL) {
			proc_lockClear(&a->lock);
		}
		proc_lockClear(&amap->lock);
		return NULL;
	}

	if (a != NULL || o != NULL) {
		/* Copy from object or shared anon */
		if ((p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP)) == NULL) {
			amap_unmap(map, v);
			if (a != NULL) {
				proc_lockClear(&a->lock);
			}
			proc_lockClear(&amap->lock);
			return NULL;
		}
		if ((w = amap_map(map, p)) == NULL) {
			vm_pageFree(p);
			amap_unmap(map, v);
			if (a != NULL) {
				proc_lockClear(&a->lock);
			}
			proc_lockClear(&amap->lock);
			return NULL;
		}
		hal_memcpy(w, v, SIZE_PAGE);
		amap_unmap(map, w);
	}
	else {
		hal_memset(v, 0, SIZE_PAGE);
	}

	amap_unmap(map, v);

	if (a != NULL) {
		proc_lockClear(&a->lock);
	}

	if ((amap->anons[aoffs / SIZE_PAGE] = anon_new(p)) == NULL) {
		vm_pageFree(p);
		p = NULL;
	}
	proc_lockClear(&amap->lock);

	return p;
}


void _amap_init(vm_map_t *kmap, vm_object_t *kernel)
{
	amap_common.kmap = kmap;
	amap_common.kernel = kernel;
}
