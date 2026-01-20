/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Virtual memory manager
 *
 * Copyright 2012, 2016-2017 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "lib/lib.h"
#include "include/sysinfo.h"
#include "phmap.h"
#include "map.h"
#include "amap.h"
#include "zone.h"
#include "kmalloc.h"
#include "vm/vm.h"


static struct {
	void *bss;
	void *top;
} vm;


void vm_meminfo(meminfo_t *info)
{
	vm_phinfo(info);
	vm_mapinfo(info);
}


void _vm_init(vm_map_t *kmap, vm_object_t *kernel)
{
	/* parasoft-suppress-next-line MISRAC2012-RULE_18_1 "&vm.top is passed as a reference to vm.top not as an array object" */
	_pmap_init(&kmap->pmap, &vm.bss, &vm.top);
	_vm_phmap_init(&kmap->pmap, &vm.bss, &vm.top);

	(void)_map_init(kmap, kernel, &vm.bss, &vm.top);

	_zone_init(kmap, kernel, &vm.bss, &vm.top);
	(void)_kmalloc_init();

	(void)_object_init(kmap, kernel);
	_amap_init(kmap, kernel);

	return;
}
