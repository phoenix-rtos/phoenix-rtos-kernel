/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Fine-grained memory allocator
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "lib/lib.h"
#include "map.h"
#include "zone.h"
#include "include/errno.h"
#include "proc/proc.h"


static struct {
	vm_zone_t *sizes[24];
	vm_zone_t *used;
	vm_zone_t firstzone;

	rbtree_t tree;

	unsigned int hdrblocks;
	size_t allocsz;

	unsigned int zonehdrs;
	lock_t lock;
} kmalloc_common;


static int kmalloc_zone_cmp(rbnode_t *n1, rbnode_t *n2)
{
	vm_zone_t *z1 = lib_treeof(vm_zone_t, linkage, n1);
	vm_zone_t *z2 = lib_treeof(vm_zone_t, linkage, n2);

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "Variable pass to lib_treeof will not be NULL, so lib_treeof will not be NULL either" */
	if ((ptr_t)z1->vaddr > (ptr_t)z2->vaddr) {
		return 1;
	}

	if ((((ptr_t)z2->vaddr >= (ptr_t)z1->vaddr) && ((ptr_t)z2->vaddr < (ptr_t)z1->vaddr + z1->blocks * z1->blocksz)) ||
			(((ptr_t)z1->vaddr >= (ptr_t)z2->vaddr) && ((ptr_t)z1->vaddr < (ptr_t)z2->vaddr + z2->blocks * z2->blocksz))) {
		return 0;
	}

	return -1;
}


static void *_kmalloc_alloc(u8 hdridx, u8 idx)
{
	void *b;
	vm_zone_t *z = kmalloc_common.sizes[idx];

	b = _vm_zalloc(z, NULL);
	if (b != NULL) {
		kmalloc_common.allocsz += (1UL << idx);

		if (idx == hdridx) {
			kmalloc_common.hdrblocks--;
		}

		if (z->used == z->blocks) {
			LIST_REMOVE(&kmalloc_common.sizes[idx], z);
			LIST_ADD(&kmalloc_common.used, z);
		}
	}

	return b;
}


static vm_zone_t *_kmalloc_free(u8 hdridx, void *p)
{
	vm_zone_t t;
	vm_zone_t *z;
	u8 idx;

	/* Free block */
	t.vaddr = p;
	t.blocks = 1;
	t.blocksz = 16;

	if ((z = lib_treeof(vm_zone_t, linkage, lib_rbFind(&kmalloc_common.tree, &t.linkage))) == NULL) {
		return NULL;
	}

	_vm_zfree(z, p);
	kmalloc_common.allocsz -= z->blocksz;

	if ((idx = (u8)hal_cpuGetLastBit(z->blocksz)) == hdridx) {
		kmalloc_common.hdrblocks++;
	}

	/* Remove zone from used list */
	if (z->used == z->blocks - 1U) {
		LIST_REMOVE(&kmalloc_common.used, z);
		LIST_ADD(&kmalloc_common.sizes[idx], z);
	}

	return z;
}


static int _kmalloc_addZone(u8 hdridx, u8 idx)
{
	vm_zone_t *nz;
	size_t blocksz;

	nz = _kmalloc_alloc(hdridx, hdridx);
	if (nz == NULL) {
		return -ENOMEM;
	}

	/* Add new zone */
	blocksz = 0x1UL << idx;
	if (_vm_zoneCreate(nz, blocksz, max(((idx == hdridx) ? kmalloc_common.zonehdrs : 1U), SIZE_PAGE / blocksz)) < 0) {
		(void)_kmalloc_free(hdridx, nz);
		return -ENOMEM;
	}

	LIST_ADD(&kmalloc_common.sizes[idx], nz);
	(void)lib_rbInsert(&kmalloc_common.tree, &nz->linkage);

	if (idx == hdridx) {
		kmalloc_common.hdrblocks += nz->blocks;
	}

	return EOK;
}


void *vm_kmalloc(size_t size)
{
	u8 idx, hdridx;
	void *b = NULL;
	vm_zone_t *z;
	int err = EOK;

	/* Establish minimal size */
	size = size < 16U ? 16U : size;

	idx = (u8)hal_cpuGetLastBit(size);
	if ((u8)hal_cpuGetFirstBit(size) < idx) {
		idx++;
	}
	if (idx >= sizeof(kmalloc_common.sizes) / sizeof(vm_zone_t *)) {
		return NULL;
	}

	hdridx = (u8)hal_cpuGetLastBit(sizeof(vm_zone_t));
	if ((u8)hal_cpuGetFirstBit(sizeof(vm_zone_t)) < hdridx) {
		hdridx++;
	}
	if (hdridx >= sizeof(kmalloc_common.sizes) / sizeof(vm_zone_t *)) {
		return NULL;
	}

	(void)proc_lockSet(&kmalloc_common.lock);

	if (kmalloc_common.hdrblocks == 1U) {
		err = _kmalloc_addZone(hdridx, hdridx);
	}

	if (err == 0) {
		z = kmalloc_common.sizes[idx];
		if (z == NULL) {
			err = _kmalloc_addZone(hdridx, idx);
		}
	}

	/* Alloc new fragment */
	if (err == 0) {
		b = _kmalloc_alloc(hdridx, idx);
	}

	(void)proc_lockClear(&kmalloc_common.lock);

	return b;
}


static void *_kmalloc_freeAtom(u8 hdridx, void *p)
{
	vm_zone_t *z;
	u8 idx;

	z = _kmalloc_free(hdridx, p);
	if (z == NULL) {
		return NULL;
	}

	idx = (u8)hal_cpuGetLastBit(z->blocksz);

	/* Remove zone if free */
	if ((z->used == 0U) && (z != &kmalloc_common.firstzone)) {
		LIST_REMOVE(&kmalloc_common.sizes[idx], z);
		(void)_vm_zoneDestroy(z);
		lib_rbRemove(&kmalloc_common.tree, &z->linkage);

		if (idx == hdridx) {
			kmalloc_common.hdrblocks -= z->blocks;
		}
		return z;
	}

	return NULL;
}


void vm_kfree(void *p)
{
	u8 hdridx;

	hdridx = (u8)hal_cpuGetLastBit(sizeof(vm_zone_t));
	if ((u8)hal_cpuGetFirstBit(sizeof(vm_zone_t)) < hdridx) {
		hdridx++;
	}
	if (hdridx >= sizeof(kmalloc_common.sizes) / sizeof(vm_zone_t *)) {
		return;
	}

	(void)proc_lockSet(&kmalloc_common.lock);

	while (p != NULL) {
		p = _kmalloc_freeAtom(hdridx, p);
	}

	(void)proc_lockClear(&kmalloc_common.lock);
}


void vm_kmallocGetStats(size_t *allocsz)
{
	*allocsz = kmalloc_common.allocsz;
}


void vm_kmallocDump(void)
{
	unsigned int i;
	vm_zone_t *z;

	for (i = 0; i < sizeof(kmalloc_common.sizes) / sizeof(vm_zone_t *); i++) {
		lib_printf("sizes[%d]=", i);
		z = kmalloc_common.sizes[i];

		if (z != NULL) {
			do {
				lib_printf("%p(%d/%d) ", z, z->used, z->blocks);
				z = z->next;
			} while (z != kmalloc_common.sizes[i]);
		}

		lib_printf("\n");
	}
}


int _kmalloc_init(void)
{
	unsigned int hdridx, i;
	size_t blocksz;

	lib_printf("vm: Initializing kernel memory allocator: ");

	(void)proc_lockInit(&kmalloc_common.lock, &proc_lockAttrDefault, "kmalloc.common");

	hdridx = hal_cpuGetLastBit(sizeof(vm_zone_t));
	if (hal_cpuGetFirstBit(sizeof(vm_zone_t)) < hdridx) {
		hdridx++;
	}
	if (hdridx >= sizeof(kmalloc_common.sizes) / sizeof(vm_zone_t *)) {
		lib_printf("BAD HDRIDX!\n");
		return -1;
	}

	/* Initialize sizes */
	for (i = 0; i < sizeof(kmalloc_common.sizes) / sizeof(vm_zone_t *); i++) {
		kmalloc_common.sizes[i] = NULL;
	}
	kmalloc_common.used = NULL;

	/* Initialize allocated zone tree */
	lib_rbInit(&kmalloc_common.tree, kmalloc_zone_cmp, NULL);

	kmalloc_common.zonehdrs = 16;

	/* Add first zone_t zone */
	blocksz = 0x1UL << hdridx;
	(void)_vm_zoneCreate(&kmalloc_common.firstzone, blocksz, max(kmalloc_common.zonehdrs, SIZE_PAGE / blocksz));
	LIST_ADD(&kmalloc_common.sizes[hdridx], &kmalloc_common.firstzone);
	(void)lib_rbInsert(&kmalloc_common.tree, &kmalloc_common.firstzone.linkage);

	kmalloc_common.allocsz = 0;
	kmalloc_common.hdrblocks = kmalloc_common.firstzone.blocks;

	lib_printf("(%d*%d) %d\n", kmalloc_common.hdrblocks, sizeof(vm_zone_t), kmalloc_common.hdrblocks * sizeof(vm_zone_t));

	return 0;
}
