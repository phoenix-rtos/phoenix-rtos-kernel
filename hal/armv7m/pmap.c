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
#include "lib/lib.h"
#include <arch/cpu.h>
#include <arch/spinlock.h>


#define MPU_BASE ((volatile u32 *)0xe000ed90U)


/* clang-format off */
enum { mpu_type, mpu_ctrl, mpu_rnr, mpu_rbar, mpu_rasr, mpu_rbar_a1, mpu_rasr_a1, mpu_rbar_a2, mpu_rasr_a2,
	mpu_rbar_a3, mpu_rasr_a3 };
/* clang-format on */

/* parasoft-begin-suppress MISRAC2012-RULE_8_6 "Provided by toolchain" */
/* Linker symbols */
extern unsigned int _end;
extern unsigned int __bss_start;
/* parasoft-end-suppress MISRAC2012-RULE_8_6 */

/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Definition in assembly" */
extern void *_init_vectors;

static struct {
	volatile u32 *mpu;
	spinlock_t lock;
	unsigned int last_mpu_count;
} pmap_common;


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, const syspage_prog_t *prog, void *vaddr)
{
	if (prog != NULL) {
		pmap->hal = &prog->partition->hal;
	}
	else {
		pmap->hal = NULL;
	}
	return 0;
}


addr_t pmap_destroy(pmap_t *pmap, unsigned int *i)
{
	return 0;
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3-a "Optimized context switching code" */
void pmap_switch(pmap_t *pmap)
{
	static const volatile u32 *RBAR_ADDR = MPU_BASE + mpu_rbar;
	unsigned int allocCnt;
	spinlock_ctx_t sc;
	unsigned int i;
	const u32 *tableCurrent;

	if (pmap != NULL && pmap->hal != NULL) {
		hal_spinlockSet(&pmap_common.lock, &sc);

		allocCnt = pmap->hal->mpu.allocCnt;
		tableCurrent = &pmap->hal->mpu.table[0].rbar;

		/* Disable MPU */
		hal_cpuDataMemoryBarrier();
		*(pmap_common.mpu + mpu_ctrl) &= ~1U;

		for (i = 0; i < max(allocCnt, pmap_common.last_mpu_count); i += 4U) {
			/* region number update is done by writes to RBAR */
			__asm__ volatile(
					"ldmia %[tableCurrent]!, {r3-r8, r10, r11} \n\t" /* Load 4 regions (rbar/rasr pairs) from table, update table pointer */
					"stmia %[mpu_rbar], {r3-r8, r10, r11}      \n\t" /* Write 4 regions via RBAR/RASR and aliases */
					: [tableCurrent] "+&r"(tableCurrent)
					: [mpu_rbar] "r"(RBAR_ADDR)
					: "r3", "r4", "r5", "r6", "r7", "r8", "r10", "r11");
		}

		/* Enable MPU */
		*(pmap_common.mpu + mpu_ctrl) |= 1U;
		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();

		pmap_common.last_mpu_count = allocCnt;

		hal_spinlockClear(&pmap_common.lock, &sc);
	}
}


int pmap_enter(pmap_t *pmap, addr_t paddr, void *vaddr, vm_attr_t attr, page_t *alloc)
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
	unsigned int i;
	const syspage_map_t *map = syspage_mapAddrResolve((addr_t)vaddr);
	if (map == NULL) {
		return 0;
	}

	if (pmap->hal == NULL) {
		/* Kernel pmap has access to everything */
		return 1;
	}

	for (i = 0; i < pmap->hal->mpu.allocCnt; ++i) {
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
	if (i != 0U) {
		return -1;
	}

	/* Returns region above basic kernel's .bss section */
	*vaddr = (void *)&_end;
	*size = (((size_t)(*top) + SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U)) - (size_t)&_end;

	return 0;
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	unsigned int cnt = (syspage->hs.mpuType >> 8U) & 0xffU;
	unsigned int i;

	(*vstart) = (void *)(((ptr_t)_init_vectors + 7U) & ~7U);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */
	pmap->end = (void *)((addr_t)&__bss_start + 32U * 1024U);

	pmap->hal = NULL;
	pmap_common.last_mpu_count = cnt;

	/* Configure MPU */
	pmap_common.mpu = MPU_BASE;

	/* Disable MPU just in case */
	*(pmap_common.mpu + mpu_ctrl) &= ~1U;
	hal_cpuDataMemoryBarrier();

	/* Allow unlimited kernel access */
	*(pmap_common.mpu + mpu_ctrl) |= (1U << 2);
	hal_cpuDataMemoryBarrier();

	for (i = 0; i < cnt; ++i) {
		/* Select region */
		*(pmap_common.mpu + mpu_rnr) = i;

		/* Disable all regions for now */
		*(pmap_common.mpu + mpu_rasr) = 0U;
		hal_cpuDataMemoryBarrier();
	}

	/* Enable MPU */
	*(pmap_common.mpu + mpu_ctrl) |= 1U;
	hal_cpuDataMemoryBarrier();

	hal_spinlockCreate(&pmap_common.lock, "pmap");
}
