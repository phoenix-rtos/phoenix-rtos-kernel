/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem (ARMv7 with MPU)
 *
 * Copyright 2017, 2020-2021 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/pmap.h"
#include "config.h"
#include "syspage.h"
#include "halsyspage.h"
#include <arch/cpu.h>
#include "armv7m.h"

enum { mpu_type, mpu_ctrl, mpu_rnr, mpu_rbar, mpu_rasr, mpu_rbar_a1, mpu_rasr_a1, mpu_rbar_a2, mpu_rasr_a2,
	   mpu_rbar_a3, mpu_rasr_a3 };

/* Linker symbols */
extern unsigned int _end;
extern unsigned int __bss_start;

extern void *_init_vectors;

static struct {
	volatile u32 *mpu;
	unsigned int kernelCodeRegion;
} pmap_common;


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	pmap->regions = pmap_common.kernelCodeRegion;
	return 0;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	return 0;
}


static unsigned int pmap_map2region(unsigned int map)
{
	int i;
	unsigned int mask = 0;

	for (i = 0; i < sizeof(syspage->hs.mpu.map) / sizeof(*syspage->hs.mpu.map); ++i) {
		if (map == syspage->hs.mpu.map[i]) {
			mask |= (1 << i);
		}
	}

	return mask;
}


int pmap_addMap(pmap_t *pmap, unsigned int map)
{
	unsigned int rmask = pmap_map2region(map);
	if (rmask == 0) {
		return -1;
	}

	pmap->regions |= rmask;

	return 0;
}


void pmap_switch(pmap_t *pmap)
{
	unsigned int i, cnt = syspage->hs.mpu.allocCnt;

	if (pmap != NULL) {
		for (i = 0; i < cnt; ++i) {
			/* Select region */
			*(pmap_common.mpu + mpu_rnr) = i;
			hal_cpuDataMemoryBarrier();

			/* Enable/disable region according to the mask */
			if ((pmap->regions & (1 << i)) != 0) {
				*(pmap_common.mpu + mpu_rasr) |= 1;
			}
			else {
				*(pmap_common.mpu + mpu_rasr) &= ~1;
			}
			hal_cpuDataMemoryBarrier();
		}
	}
}


int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc)
{
	return 0;
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	return 0;
}


addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	return (addr_t)vaddr;
}


int pmap_isAllowed(pmap_t *pmap, const void *vaddr, size_t size)
{
	const syspage_map_t *map = syspage_mapAddrResolve((addr_t)vaddr);
	unsigned int rmask;
	if (map == NULL) {
		return 0;
	}
	rmask = pmap_map2region(map->id);

	return ((pmap->regions & rmask) == 0) ? 0 : 1;
}


int pmap_getPage(page_t *page, addr_t *addr)
{
	return 0;
}


char pmap_marker(page_t *p)
{
	return 0;
}


int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp)
{
	return 0;
}


int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top)
{
	if (i)
		return -1;

	/* Returns region above basic kernel's .bss section */
	*vaddr = (void *)&_end;
	*size = (((size_t)(*top) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1)) - (size_t)&_end;

	return 0;
}


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	const syspage_map_t *ikmap;
	unsigned int ikregion;
	u32 t;
	addr_t pc;
	unsigned int i, cnt = syspage->hs.mpu.allocCnt;

	(*vstart) = (void *)(((ptr_t)_init_vectors + 7) & ~7);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */
	pmap->end = (void *)((addr_t)&__bss_start + 32 * 1024);

	/* Configure MPU */
	pmap_common.mpu = (void *)0xe000ed90;

	/* Disable MPU just in case */
	*(pmap_common.mpu + mpu_ctrl) &= ~1;
	hal_cpuDataMemoryBarrier();

	/* Allow unlimited kernel access */
	*(pmap_common.mpu + mpu_ctrl) |= (1 << 2);
	hal_cpuDataMemoryBarrier();

	for (i = 0; i < cnt; ++i) {
		t = syspage->hs.mpu.table[i].rbar;
		if ((t & (1 << 4)) == 0) {
			continue;
		}

		*(pmap_common.mpu + mpu_rbar) = t;
		hal_cpuDataMemoryBarrier();

		/* Disable regions for now */
		t = syspage->hs.mpu.table[i].rasr & ~1;
		*(pmap_common.mpu + mpu_rasr) = t;
		hal_cpuDataMemoryBarrier();
	}

	/* Enable MPU */
	*(pmap_common.mpu + mpu_ctrl) |= 1;
	hal_cpuDataMemoryBarrier();

	/* FIXME HACK
	 * allow all programs to execute (and read) kernel code map.
	 * Needed because of hal_jmp, syscalls handler and signals handler.
	 * In these functions we need to switch to the user mode when still
	 * executing kernel code. This will cause memory management fault
	 * if the application does not have access to the kernel instruction
	 * map. Possible fix - place return to the user code in the separate
	 * region and allow this region instead. */

	/* Find kernel code region */
	__asm__ volatile ("\tmov %0, pc;" : "=r" (pc));
	ikmap = syspage_mapAddrResolve(pc);
	if (ikmap == NULL) {
		hal_consolePrint(ATTR_BOLD, "pmap: Kernel code map not found. Bad system config\n");
		for (;;) {
			hal_cpuHalt();
		}
	}

	ikregion = pmap_map2region(ikmap->id);
	if (ikregion == 0) {
		hal_consolePrint(ATTR_BOLD, "pmap: Kernel code map has no assigned region. Bad system config\n");
		for (;;) {
			hal_cpuHalt();
		}
	}

	pmap_common.kernelCodeRegion = ikregion;
}
