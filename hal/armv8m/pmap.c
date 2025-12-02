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
#include "lib/lib.h"
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
	int last_mpu_count;
} pmap_common;


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, const syspage_prog_t *prog, void *vaddr)
{
	if (prog != NULL) {
		pmap->hal = &prog->hal;
	}
	else {
		pmap->hal = NULL;
	}
	return 0;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	return 0;
}


void pmap_switch(pmap_t *pmap)
{
	static const volatile u32 *RBAR_ADDR = MPU_BASE + mpu_rbar;
	unsigned int allocCnt;
	spinlock_ctx_t sc;
	unsigned int i;
	const u32 *tableCurrent;

	if (pmap_common.mpu_enabled == 0) {
		return;
	}

	if (pmap != NULL && pmap->hal != NULL) {
		hal_spinlockSet(&pmap_common.lock, &sc);

		allocCnt = pmap->hal->mpu.allocCnt;
		tableCurrent = &pmap->hal->mpu.table[0].rbar;

		/* Disable MPU */
		hal_cpuDataMemoryBarrier();
		*(pmap_common.mpu + mpu_ctrl) &= ~1;

		for (i = 0; i < max(allocCnt, pmap_common.last_mpu_count); i += 4) {
			*(pmap_common.mpu + mpu_rnr) = i;
			__asm__ volatile(
					"ldmia %[tableCurrent]!, {r3-r8, r10, r11} \n\t" /* Load 4 regions (rbar/rlar pairs) from table, update table pointer */
					"stmia %[mpu_rbar], {r3-r8, r10, r11}      \n\t" /* Write 4 regions via RBAR/RLAR and aliases */
					: [tableCurrent] "+r"(tableCurrent)
					: [mpu_rbar] "r"(RBAR_ADDR)
					: "r3", "r4", "r5", "r6", "r7", "r8", "r10", "r11");
		}

		/* Enable MPU */
		*(pmap_common.mpu + mpu_ctrl) |= 1;
		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();

		pmap_common.last_mpu_count = allocCnt;

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

	if (pmap->hal == NULL) {
		/* Kernel pmap has access to everything */
		return 1;
	}

	for (int i = 0; i < pmap->hal->mpu.allocCnt; ++i) {
		if (pmap->hal->mpu.map[i] == map->id) {
			return 1;
		}
	}
	return 0;
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
	unsigned int cnt = min(
			(syspage->hs.mpuType >> 8U) & 0xffU,
			sizeof(pmap->hal->mpu.table) / sizeof(pmap->hal->mpu.table[0]));
	int i;

	(*vstart) = (void *)(((ptr_t)_init_vectors + 7) & ~7U);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */
	pmap->end = (void *)((addr_t)&__bss_start + 32 * 1024);

	pmap->hal = NULL;
	pmap_common.last_mpu_count = cnt;

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

		/* Disable all regions for now */
		*(pmap_common.mpu + mpu_rlar) = 0;
	}

	/* Enable MPU */
	*(pmap_common.mpu + mpu_ctrl) |= 1;
	hal_cpuDataSyncBarrier();
}
