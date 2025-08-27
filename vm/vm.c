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
#include "page.h"
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
	vm_pageinfo(info);
	vm_mapinfo(info);
}


void _vm_init(vm_map_t *kmap, vm_object_t *kernel)
{
	_pmap_init(&kmap->pmap, &vm.bss, &vm.top);
	_page_init(&kmap->pmap, &vm.bss, &vm.top);

	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)_map_init(kmap, kernel, &vm.bss, &vm.top);

	_zone_init(kmap, kernel, &vm.bss, &vm.top);
	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)_kmalloc_init();

	/* MISRA Rule 17.7: Unused returned value, (void) added */
	(void)_object_init(kmap, kernel);
	_amap_init(kmap, kernel);

	return;
}
