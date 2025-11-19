/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager - object management
 *
 * Copyright 2017, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk, Jan Sikorski, Maciej Purski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "include/errno.h"
#include "lib/lib.h"
#include "page.h"
#include "kmalloc.h"
#include "object.h"
#include "map.h"
#include "proc/name.h"
#include "proc/threads.h"


static struct {
	rbtree_t tree;
	vm_object_t *kernel;
	vm_map_t *kmap;
	lock_t lock;
} object_common;


static int object_cmp(rbnode_t *n1, rbnode_t *n2)
{
	vm_object_t *o1 = lib_treeof(vm_object_t, linkage, n1);
	vm_object_t *o2 = lib_treeof(vm_object_t, linkage, n2);

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
	if (o1->oid.id > o2->oid.id) {
		return 1;
	}
	if (o1->oid.id < o2->oid.id) {
		return -1;
	}

	if (o1->oid.port > o2->oid.port) {
		return 1;
	}
	if (o1->oid.port < o2->oid.port) {
		return -1;
	}

	return 0;
}


int vm_objectGet(vm_object_t **o, oid_t oid)
{
	vm_object_t t, *no = NULL;
	size_t i, n;
	off_t sz;
	int err = -ENOMEM;

	t.oid.port = oid.port;
	t.oid.id = oid.id;

	(void)proc_lockSet(&object_common.lock);
	*o = lib_treeof(vm_object_t, linkage, lib_rbFind(&object_common.tree, &t.linkage));

	if (*o == NULL) {
		/* Take off the lock to avoid a deadlock in vm_kmalloc */
		(void)proc_lockClear(&object_common.lock);

		sz = proc_size(oid);
		if (sz < 0) {
			err = (int)sz;
		}
		else if ((sizeof(off_t) <= sizeof(size_t)) || (sz <= (off_t)((size_t)-1))) {
			n = round_page((size_t)sz) / SIZE_PAGE;
			no = (vm_object_t *)vm_kmalloc(sizeof(vm_object_t) + n * sizeof(page_t *));
		}
		else {
			/* No action required */
		}


		(void)proc_lockSet(&object_common.lock);
		/* Check again, somebody could've added the object in the meantime */
		*o = lib_treeof(vm_object_t, linkage, lib_rbFind(&object_common.tree, &t.linkage));
		if (*o == NULL) {
			if (no == NULL) {
				(void)proc_lockClear(&object_common.lock);
				return err;
			}
			*o = no;
			no = NULL;
			hal_memcpy(&(*o)->oid, &oid, sizeof(oid));

			/* Safe to cast - sz fits into size_t from above checks */
			(*o)->size = (size_t)sz;
			(*o)->refs = 0;

			for (i = 0; i < n; ++i) {
				(*o)->pages[i] = NULL;
			}

			(void)lib_rbInsert(&object_common.tree, &(*o)->linkage);
		}
	}

	(*o)->refs++;
	(void)proc_lockClear(&object_common.lock);

	/* Did we allocate an object we didn't need in the end? */
	if (no != NULL) {
		vm_kfree(no);
	}

	return EOK;
}


vm_object_t *vm_objectRef(vm_object_t *o)
{
	if ((o != NULL) && (o != VM_OBJ_PHYSMEM)) {
		(void)proc_lockSet(&object_common.lock);
		o->refs++;
		(void)proc_lockClear(&object_common.lock);
	}

	return o;
}


int vm_objectPut(vm_object_t *o)
{
	unsigned int i;

	if ((o == NULL) || (o == VM_OBJ_PHYSMEM)) {
		return EOK;
	}

	(void)proc_lockSet(&object_common.lock);

	if (--o->refs != 0) {
		(void)proc_lockClear(&object_common.lock);
		return EOK;
	}

	lib_rbRemove(&object_common.tree, &o->linkage);
	(void)proc_lockClear(&object_common.lock);

	/* Contiguous object 'holds' all pages in pages[0] */
	if ((o->oid.port == (u32)(-1)) && (o->oid.id == (id_t)(-1))) {
		vm_pageFree(o->pages[0]);
	}
	else {
		for (i = 0; i < round_page(o->size) / SIZE_PAGE; ++i) {
			if (o->pages[i] != NULL) {
				vm_pageFree(o->pages[i]);
			}
		}
	}

	vm_kfree(o);

	return EOK;
}


static page_t *object_fetch(oid_t oid, u64 offs)
{
	page_t *p;
	void *v;

	if (proc_open(oid, 0) < 0) {
		return NULL;
	}

	p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
	if (p == NULL) {
		(void)proc_close(oid, 0);
		return NULL;
	}

	v = vm_mmap(object_common.kmap, NULL, p, SIZE_PAGE, PROT_WRITE | PROT_USER, object_common.kernel, 0, MAP_NONE);
	if (v == NULL) {
		vm_pageFree(p);
		(void)proc_close(oid, 0);
		return NULL;
	}

	if (proc_read(oid, (off_t)offs, v, SIZE_PAGE, 0) < 0) {
		(void)vm_munmap(object_common.kmap, v, SIZE_PAGE);
		vm_pageFree(p);
		(void)proc_close(oid, 0);
		return NULL;
	}

	(void)vm_munmap(object_common.kmap, v, SIZE_PAGE);
	(void)proc_close(oid, 0);

	return p;
}


page_t *vm_objectPage(vm_map_t *map, amap_t **amap, vm_object_t *o, void *vaddr, u64 offs)
{
	page_t *p;

	if (o == NULL) {
		return vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
	}

	if (o == VM_OBJ_PHYSMEM) {
		if (offs > (addr_t)-1) {
			return NULL;
		}
		return _page_get((addr_t)offs);
	}

	(void)proc_lockSet(&object_common.lock);

	if (offs >= o->size) {
		(void)proc_lockClear(&object_common.lock);
		return NULL;
	}

	p = o->pages[offs / SIZE_PAGE];
	if (p != NULL) {
		(void)proc_lockClear(&object_common.lock);
		return p;
	}

	/* Fetch page from backing store */

	(void)proc_lockClear(&object_common.lock);

	if (amap != NULL) {
		(void)proc_lockClear(&(*amap)->lock);
	}

	(void)proc_lockClear(&map->lock);

	p = object_fetch(o->oid, offs);

	if (vm_lockVerify(map, amap, o, vaddr, offs) != 0) {
		if (p != NULL) {
			vm_pageFree(p);
		}

		return NULL;
	}

	(void)proc_lockSet(&object_common.lock);

	if (o->pages[offs / SIZE_PAGE] != NULL) {
		/* Someone loaded a page in the meantime, use it */
		if (p != NULL) {
			vm_pageFree(p);
		}

		p = o->pages[offs / SIZE_PAGE];
		(void)proc_lockClear(&object_common.lock);
		return p;
	}

	o->pages[offs / SIZE_PAGE] = p;
	(void)proc_lockClear(&object_common.lock);
	return p;
}


vm_object_t *vm_objectContiguous(size_t size)
{
	vm_object_t *o;
	page_t *p;
	unsigned int i, n;

	p = vm_pageAlloc(size, PAGE_OWNER_APP);
	if (p == NULL) {
		return NULL;
	}

	size = 1UL << p->idx;
	n = size / SIZE_PAGE;

	o = vm_kmalloc(sizeof(vm_object_t) + n * sizeof(page_t *));
	if (o == NULL) {
		vm_pageFree(p);
		return NULL;
	}

	hal_memset(o, 0, sizeof(*o));
	/* Mark object as contiguous by setting its oid.port and oid.id to -1 */
	o->oid.port = (u32)(-1);
	o->oid.id = (id_t)(-1);
	o->refs = 1;
	o->size = size;

	for (i = 0; i < n; ++i) {
		o->pages[i] = p + i;
	}

	return o;
}


int _object_init(vm_map_t *kmap, vm_object_t *kernel)
{
	vm_object_t *o;

	lib_printf("vm: Initializing memory objects\n");

	object_common.kernel = kernel;
	object_common.kmap = kmap;

	(void)proc_lockInit(&object_common.lock, &proc_lockAttrDefault, "object.common");
	lib_rbInit(&object_common.tree, object_cmp, NULL);

	kernel->refs = 0;
	kernel->oid.port = 0;
	kernel->oid.id = 0;
	(void)lib_rbInsert(&object_common.tree, &kernel->linkage);

	(void)vm_objectGet(&o, kernel->oid);

	return EOK;
}
