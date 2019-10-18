/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - object management
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "../../include/errno.h"
#include "../../include/fcntl.h"
#include "../../include/msg.h"
#include "../lib/lib.h"
#include "page.h"
#include "kmalloc.h"
#include "object.h"
#include "map.h"
#include "../proc/server.h"
#include "../proc/name.h"
#include "../proc/threads.h"


struct {
	rbtree_t tree;
	vm_object_t *kernel;
	vm_map_t *kmap;
	lock_t lock;
} object_common;


static int object_cmp(rbnode_t *n1, rbnode_t *n2)
{
	vm_object_t *o1 = lib_treeof(vm_object_t, linkage, n1);
	vm_object_t *o2 = lib_treeof(vm_object_t, linkage, n2);

	if (o1->oid.id > o2->oid.id)
		return 1;
	if (o1->oid.id < o2->oid.id)
		return -1;

	if (o1->oid.port > o2->oid.port)
 		return 1;
	if (o1->oid.port < o2->oid.port)
		return -1;

	return 0;
}


int vm_objectGet(vm_object_t **o, oid_t oid)
{
	vm_object_t t;
	int i, n;
	size_t sz;
	port_t *port;

	t.oid.port = oid.port;
	t.oid.id = oid.id;

	proc_lockSet(&object_common.lock);
	*o = lib_treeof(vm_object_t, linkage, lib_rbFind(&object_common.tree, &t.linkage));

	if (*o == NULL) {
		if ((port = port_get(oid.port)) == NULL) {
			proc_lockClear(&object_common.lock);
			return -ENXIO;
		}

		if (proc_objectGetAttr(port, oid.id, atSize, &sz, sizeof(sz)) != sizeof(sz)) {
			proc_lockClear(&object_common.lock);
			port_put(port);
			return -EINVAL; /* other error? */
		}

		port_put(port);
		n = round_page(sz) / SIZE_PAGE;

		if ((*o = (vm_object_t *)vm_kmalloc(sizeof(vm_object_t) + n * sizeof(page_t *))) == NULL) {
			proc_lockClear(&object_common.lock);
			return -ENOMEM;
		}

		hal_memcpy(&(*o)->oid, &oid, sizeof(oid));
		(*o)->size = sz;
		(*o)->refs = 0;
		proc_lockInit(&(*o)->lock);

		for (i = 0; i < n; ++i)
			(*o)->pages[i] = NULL;

		lib_rbInsert(&object_common.tree, &(*o)->linkage);
	}

	(*o)->refs++;
	proc_lockClear(&object_common.lock);

	return EOK;
}


vm_object_t *vm_objectRef(vm_object_t *o)
{
	if (o != NULL && o != (void *)-1) {
		proc_lockSet(&o->lock);
		o->refs++;
		proc_lockClear(&o->lock);
	}

	return o;
}


int vm_objectPut(vm_object_t *o)
{
	int i;

	if (o == NULL || o == (void *)-1)
		return EOK;

	proc_lockSet(&o->lock);

	if (--o->refs) {
		proc_lockClear(&o->lock);
		return EOK;
	}

	proc_lockSet(&object_common.lock);
	lib_rbRemove(&object_common.tree, &o->linkage);
	proc_lockClear(&object_common.lock);

	proc_lockDone(&o->lock);

	for (i = 0; i < round_page(o->size) / SIZE_PAGE; ++i) {
		if (o->pages[i] != NULL)
			vm_pageFree(o->pages[i]);
	}

	vm_kfree(o);

	return EOK;
}


static page_t *object_fetch(oid_t oid, offs_t offs)
{
	page_t *p;
	void *v;
	port_t *port;

	if ((p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP)) == NULL)
		return NULL;

	if ((v = vm_mmap(object_common.kmap, NULL, p, SIZE_PAGE, PROT_WRITE | PROT_USER, object_common.kernel, 0, MAP_NONE)) == NULL) {
		vm_pageFree(p);
		return NULL;
	}

	if ((port = port_get(oid.port)) == NULL) {
		vm_munmap(object_common.kmap, v, SIZE_PAGE);
		vm_pageFree(p);
		return NULL;		
	}

	if (proc_objectRead(port, oid.id, v, SIZE_PAGE, offs) <= 0) {
		vm_munmap(object_common.kmap, v, SIZE_PAGE);
		vm_pageFree(p);
		port_put(port);
		return NULL;
	}

	vm_munmap(object_common.kmap, v, SIZE_PAGE);
	port_put(port);
	return p;
}


page_t *vm_objectPage(vm_map_t *map, amap_t **amap, vm_object_t *o, void *vaddr, offs_t offs)
{
	page_t *p;

	if (o == NULL)
		return vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);

	if (o == (void *)-1)
		return _page_get(offs);

	proc_lockSet(&o->lock);

	if (offs >= o->size) {
		proc_lockClear(&o->lock);
		return NULL;
	}

	if ((p = o->pages[offs / SIZE_PAGE]) != NULL) {
		proc_lockClear(&o->lock);
		return p;
	}

	/* Fetch page from backing store */

	proc_lockClear(&o->lock);

	if (amap != NULL)
		proc_lockClear(&(*amap)->lock);

	proc_lockClear(&map->lock);

	p = object_fetch(o->oid, offs);

	if (vm_lockVerify(map, amap, o, vaddr, offs)) {
		if (p != NULL)
			vm_pageFree(p);

		return NULL;
	}

	proc_lockSet(&o->lock);

	if (o->pages[offs / SIZE_PAGE] != NULL) {
		/* Someone loaded a page in the meantime, use it */
		if (p != NULL)
			vm_pageFree(p);

		p = o->pages[offs / SIZE_PAGE];
		proc_lockClear(&o->lock);
		return p;
	}

	o->pages[offs / SIZE_PAGE] = p;
	proc_lockClear(&o->lock);
	return p;
}


int _object_init(vm_map_t *kmap, vm_object_t *kernel)
{
	vm_object_t *o;

	lib_printf("vm: Initializing memory objects\n");

	object_common.kernel = kernel;
	object_common.kmap = kmap;

	proc_lockInit(&object_common.lock);
	lib_rbInit(&object_common.tree, object_cmp, NULL);

	kernel->oid.port = 0;
	kernel->oid.id = 0;
	lib_rbInsert(&object_common.tree, &kernel->linkage);
	proc_lockInit(&kernel->lock);

	vm_objectGet(&o, kernel->oid);

	return EOK;
}
