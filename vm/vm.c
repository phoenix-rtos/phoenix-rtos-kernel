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


void vm_partmeminfo(meminfo_t *info)
{
	size_t i = 0;
	syspage_part_t *part = syspage_partitionList();

	if (info->parts.partsz == -1) {
		return;
	}

	do {
		if (i < info->parts.partsz) {
			info->parts.part[i].userLimit = part->availableMem;
			info->parts.part[i].userUsed = part->usedMem;
			(void)hal_strncpy(info->parts.part[i].name, part->name, sizeof(info->parts.part[i].name));
		}
		i++;
		part = part->next;
	} while (part != syspage_partitionList());

	info->parts.partsz = i;
}


void vm_meminfo(meminfo_t *info)
{
	vm_pageinfo(info);
	vm_mapinfo(info);
	vm_partmeminfo(info);
}


void _vm_init(vm_map_t *kmap, vm_object_t *kernel)
{
	/* parasoft-suppress-next-line MISRAC2012-RULE_18_1 "&vm.top is passed as a reference to vm.top not as an array object" */
	_pmap_init(&kmap->pmap, &vm.bss, &vm.top);
	_page_init(&kmap->pmap, &vm.bss, &vm.top);

	(void)_map_init(kmap, kernel, &vm.bss, &vm.top);

	_zone_init(kmap, kernel, &vm.bss, &vm.top);
	(void)_kmalloc_init();

	(void)_object_init(kmap, kernel);
	_amap_init(kmap, kernel);

	return;
}
