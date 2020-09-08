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
#include "../include/errno.h"
#include "../lib/lib.h"
#include "page.h"
#include "kmalloc.h"
#include "object.h"
#include "map.h"
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

	t.oid.port = oid.port;
	t.oid.id = oid.id;

	proc_lockSet(&object_common.lock);
	*o = lib_treeof(vm_object_t, linkage, lib_rbFind(&object_common.tree, &t.linkage));

	if (*o == NULL) {
		sz = proc_size(oid);
		n = round_page(sz) / SIZE_PAGE;

		if ((*o = (vm_object_t *)vm_kmalloc(sizeof(vm_object_t) + n * sizeof(page_t *))) == NULL)
			return -ENOMEM;

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

	if (proc_open(oid, 0) < 0)
		return NULL;

	if ((p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP)) == NULL) {
		proc_close(oid, 0);
		return NULL;
	}

	if ((v = vm_mmap(object_common.kmap, NULL, p, SIZE_PAGE, PROT_WRITE | PROT_USER, object_common.kernel, 0, MAP_NONE)) == NULL) {
		vm_pageFree(p);
		proc_close(oid, 0);
		return NULL;
	}

	if (proc_read(oid, offs, v, SIZE_PAGE, 0) < 0) {
		vm_munmap(object_common.kmap, v, SIZE_PAGE);
		vm_pageFree(p);
		proc_close(oid, 0);
		return NULL;
	}

	vm_munmap(object_common.kmap, v, SIZE_PAGE);
	proc_close(oid, 0);

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

	kernel->refs = 0;
	kernel->oid.port = 0;
	kernel->oid.id = 0;
	lib_rbInsert(&object_common.tree, &kernel->linkage);
	proc_lockInit(&kernel->lock);

	vm_objectGet(&o, kernel->oid);

	return EOK;
}




#if 0
void map_pageFault(unsigned int n, exc_context_t *ctx)
{
	thread_t *thread;
	page_t *p;
	vm_entry_t *entry = NULL;
	void *vaddr, *paddr, *kaddr;

	vaddr = hal_cpuGetFaultAddr();
	paddr = (void *)((u32)vaddr & ~(SIZE_PAGE - 1));

	hal_cpuEnableInterrupts();
	thread = proc_current();
	int inKernel = ctx->eip >= VADDR_KERNEL;

	fault_log("th=%p : vaddr=%p", thread, vaddr);

// main_printf(ATTR_DEBUG, "map_pageFault: eip=%p, esp=%p, ss=%p, vaddr=%p\n", ctx->eip, ctx->esp, ctx->ss, vaddr);

	/* Verify that an entry with this vaddr exists.
	 * In code more below, we only care about whole page mappings, we don't care about verifying if requested
	 * address (vaddr) is within any of the sub-page mappings.
	 * Note that this means that when an address from outside the sub-page mapping is accessed, it will be
	 * handled harshly IF that page is not mapped yet, but it will go unnoticed if that page has been mapped already...
	 * This may lead to inconsistent behavior when testing, so it might be possible that we want to disable this check in some cases.
	 */
	if ((addr_t)vaddr >= VADDR_KERNEL || !thread->process || (entry = map_find(&thread->process->map, vaddr)) == NULL) {
		char buff[512];
		if (inKernel)
			klog_disable();

		main_printf(ATTR_FAILURE, "\nException %p at %p:\n\n", (void*)n, (void*)ctx->eip);
		hal_cpuDumpContext(buff, sizeof(buff), ctx);
		main_printf(ATTR_FAILURE, "%s", buff);

		/*	hal_disasm(buff, sizeof(buff), (void *)ctx->eip, NULL, 12);
		main_printf(ATTR_FAILURE, "\n%s", buff); */

#ifdef CONFIG_PROC
		if (!inKernel)
			proc_exit(0x100);
#endif
		if((addr_t)vaddr >= VADDR_KERNEL)
			panic("mem kernel\n");
		else if(!thread->process)
			panic("mem not proc\n");
		else if(entry == NULL)
			panic("mem proc vaddr not mapped\n");
		else
			panic("mem\n");
	}

	/* Allocate and map new page into virtual address space */
	if ((p = vm_pageAlloc(1, vm_pageAlloc)) == NULL) {
		main_printf(ATTR_ERROR, "map_pageFault: Out of memory!\n");
#ifdef CONFIG_PROC
		if (!inKernel)
			proc_exit(0x100);
#endif
		return;
	}
	pmap_enter(&thread->process->pmap, p, (void *)paddr, PGHD_PRESENT | PGHD_USER | PGHD_WRITE);

	vm_kmap(p, PGHD_WRITE | PGHD_PRESENT, &kaddr);

	/* Find all map entries that intersect with our page, and copy/fill page's contents accordingly */
	vm_map_t *map = &thread->process->map;
	u8 page_added_to_segment = 0;
	proc_mutexLock(&map->mutex);
	entry = map->entries;

	do {

		if ((paddr + SIZE_PAGE > entry->vaddr) && (paddr < entry->vaddr + entry->size))
		{
			unsigned int entry_off, page_off, copy_len;
			fault_log("  mapping=(%p,%x) %s", entry->vaddr, entry->size, entry->file ? "(file)" : "");

			if (!page_added_to_segment) {
				page_add_to_segment(&(entry->pages), p);
				page_added_to_segment = 1;
			}

			if (paddr >= entry->vaddr) {
				entry_off = paddr - entry->vaddr;
				page_off = 0;
			} else {
				entry_off = 0;
				page_off = entry->vaddr - paddr;
			}
			copy_len = min(SIZE_PAGE - page_off, entry->size - entry_off);

			if (entry->file == NULL) {
				fault_log("    memset(0) : vaddr=%p, size=%x", paddr + page_off, copy_len);
				hal_memset(kaddr + page_off, 0, copy_len);
			} else {
#ifdef CONFIG_FS
				int err, l;

				fault_log("    read file : file_off=%x, vaddr=%p, size=%x", entry->foffs + entry_off, paddr + page_off, copy_len);
				for (l = 0; l < copy_len;) {

					if ((err = fs_pread(entry->file, (char *)kaddr + page_off + l, copy_len - l, entry->foffs + entry_off + l)) < 0) {

						main_printf(ATTR_ERROR, "map_pageFault: Can't read data from file (off=%x, len=%x)\n",
									entry->foffs + entry_off + l, copy_len - l);
						proc_mutexUnlock(&map->mutex);
						vm_kunmap(kaddr);

						// FIXME:  unnoticed until it crashes?
						panic("bad elf file\n");
						return;
					}
					l += err;
				}
#endif
			}
			hal_cpuSyncCache(kaddr + page_off, copy_len);
		}
		entry = entry->next;
	} while (entry != map->entries);
	proc_mutexUnlock(&map->mutex);
	vm_kunmap(kaddr);
	return;
}
#endif
