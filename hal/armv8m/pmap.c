/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem (ARMv8)
 *
 * Copyright 2017, 2020-2022, 2025 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski, Damian Loewnau, Krzysztof Radzewicz
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
#include <arch/spinlock.h>


#define MPU_BASE ((void *)0xe000ed90)


/* clang-format off */
enum { mpu_type, mpu_ctrl, mpu_rnr, mpu_rbar, mpu_rlar, mpu_rbar_a1, mpu_rlar_a1, mpu_rbar_a2, mpu_rlar_a2,
	mpu_rbar_a3, mpu_rlar_a3, mpu_mair0 = 0xc, mpu_mair1 };
/* clang-format on */


/* Linker symbols */
extern unsigned int _end;
extern unsigned int __bss_start;


extern void *_init_vectors;


static struct {
	volatile u32 *mpu;
	unsigned int kernelCodeRegion;
	spinlock_t lock;
	int mpu_enabled;
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

	if (pmap_common.mpu_enabled == 0) {
		return 1;
	}

	for (i = 0; i < sizeof(syspage->hs.mpu.map) / sizeof(*syspage->hs.mpu.map); ++i) {
		if (map == syspage->hs.mpu.map[i]) {
			mask |= (1 << i);
		}
	}

	return mask;
}


int pmap_addMap(pmap_t *pmap, unsigned int map)
{
	unsigned int rmask;
	if (pmap_common.mpu_enabled == 0) {
		return 0;
	}

	rmask = pmap_map2region(map);
	if (rmask == 0) {
		return -1;
	}

	pmap->regions |= rmask;

	return 0;
}


void pmap_switch(pmap_t *pmap)
{
	unsigned int i, cnt = syspage->hs.mpu.allocCnt;
	spinlock_ctx_t sc;
	if (pmap_common.mpu_enabled == 0) {
		return;
	}

	if (pmap != NULL) {
		hal_spinlockSet(&pmap_common.lock, &sc);
		for (i = 0; i < cnt; ++i) {
			/* Select region */
			*(pmap_common.mpu + mpu_rnr) = i;
			hal_cpuDataMemoryBarrier();

			/* Enable/disable region according to the mask */
			if ((pmap->regions & (1 << i)) != 0) {
				*(pmap_common.mpu + mpu_rlar) |= 1U;
			}
			else {
				*(pmap_common.mpu + mpu_rlar) &= ~1U;
			}
			hal_cpuDataMemoryBarrier();
		}
		hal_spinlockClear(&pmap_common.lock, &sc);
	}
}


int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, vm_attr_t attr, page_t *alloc)
{
	return 0;
}


int pmap_remove(pmap_t *pmap, void *vstart, void *vend)
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
	addr_t addr_end = (addr_t)vaddr + size;
	/* Check for potential arithmetic overflow. `addr_end` is allowed to be 0,
	 * as it represents the top of memory. */
	int addr_overflowed = (addr_end != 0) && (addr_end < (addr_t)vaddr);
	if ((map == NULL) || (addr_end > map->end) || addr_overflowed) {
		return 0;
	}

	if (pmap_common.mpu_enabled == 0) {
		return 1;
	}

	rmask = pmap_map2region(map->id);

	return ((pmap->regions & rmask) != 0) ? 1 : 0;
}


int pmap_getPage(page_t *page, addr_t *addr)
{
	return 0;
}


char pmap_marker(page_t *p)
{
	return '\0';
}


int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp)
{
	return 0;
}


int pmap_segment(unsigned int i, void **vaddr, size_t *size, vm_prot_t *prot, void **top)
{
	if (i != 0) {
		return -1;
	}

	/* Returns region above basic kernel's .bss section */
	*vaddr = (void *)&_end;
	*size = (((size_t)(*top) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1)) - (size_t)&_end;

	return 0;
}


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	const syspage_map_t *ikmap;
	unsigned int ikregion;
	unsigned int i, cnt = syspage->hs.mpu.allocCnt;

	(*vstart) = (void *)(((ptr_t)_init_vectors + 7) & ~7U);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */
	pmap->end = (void *)((addr_t)&__bss_start + 32 * 1024);

	/* Enable all regions for kernel */
	pmap->regions = (1 << cnt) - 1;

	/* Configure MPU */
	pmap_common.mpu = MPU_BASE;

	hal_spinlockCreate(&pmap_common.lock, "pmap");
	if (cnt == 0) {
		pmap_common.mpu_enabled = 0;
		pmap_common.kernelCodeRegion = 0;
		return;
	}

	pmap_common.mpu_enabled = 1;

	/* Disable MPU just in case */
	*(pmap_common.mpu + mpu_ctrl) &= ~1;
	hal_cpuDataMemoryBarrier();

	/* Activate background region for privileged code - if an address does not belong to any enabled region,
	 * the default memory map will be used to determine memory attributes. */
	*(pmap_common.mpu + mpu_ctrl) |= (1 << 2);
	hal_cpuDataMemoryBarrier();

	for (i = 0; i < cnt; ++i) {
		/* Select MPU region to configure */
		*(pmap_common.mpu + mpu_rnr) = i;
		hal_cpuDataMemoryBarrier();

		*(pmap_common.mpu + mpu_rbar) = syspage->hs.mpu.table[i].rbar;
		hal_cpuDataMemoryBarrier();

		/* Disable regions for now */
		*(pmap_common.mpu + mpu_rlar) = syspage->hs.mpu.table[i].rlar & ~1U;
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
	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "We need address of this function in numeric type" */
	ikmap = syspage_mapAddrResolve((addr_t)_pmap_init);
	if (ikmap != NULL) {
		ikregion = pmap_map2region(ikmap->id);
	}

	if ((ikmap == NULL) || (ikregion == 0)) {
		hal_consolePrint(ATTR_BOLD, "pmap: Kernel code map not found or has no regions. Bad system config\n");
		for (;;) {
			hal_cpuHalt();
		}
	}

	pmap_common.kernelCodeRegion = ikregion;
}
