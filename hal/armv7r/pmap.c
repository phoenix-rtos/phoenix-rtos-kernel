/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem (ARMv7r)
 *
 * Copyright 2017, 2020-2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski, Damian Loewnau
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

/* parasoft-begin-suppress MISRAC2012-RULE_8_6 "Definition in assembly code" */
/* Linker symbols */
extern unsigned int _end;
extern unsigned int __bss_start;
/* parasoft-end-suppress MISRAC2012-RULE_8_6 */


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Global variable used in assembler code" */
u8 _init_stack[NUM_CPUS][SIZE_INITIAL_KSTACK] __attribute__((aligned(8)));


static struct {
	spinlock_t lock;
	int mpu_enabled;
	unsigned int last_mpu_count[NUM_CPUS];
} pmap_common;


static void pmap_mpu_setMemRegionNumber(u32 num)
{
	__asm__ volatile("mcr p15, 0, %0, c6, c2, 0" ::"r"(num));
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static void pmap_mpu_setMemRegionRasr(u32 rasr)
{
	/* ARMv7-R uses the same region attribute bits as ARMv7-M, but they are split over two registers */
	u32 rser = rasr & 0xffffU;
	u32 racr = rasr >> 16;
	__asm__ volatile("mcr p15, 0, %0, c6, c1, 2" ::"r"(rser));
	__asm__ volatile("mcr p15, 0, %0, c6, c1, 4" ::"r"(racr));
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static void pmap_mpu_setMemRegionStatus(int enable)
{
	u32 val;
	__asm__ volatile("mrc p15, 0, %0, c6, c1, 2" : "=r"(val));
	if (enable != 0) {
		val |= 0x1U;
	}
	else {
		val &= ~0x1U;
	}

	__asm__ volatile("mcr p15, 0, %0, c6, c1, 2" ::"r"(val));
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static void pmap_mpu_setMemRegionRbar(u32 addr)
{
	addr &= ~((0x1U << 5) - 1U);
	__asm__ volatile("mcr p15, 0, %0, c6, c1, 0" ::"r"(addr));
}


static void pmap_mpu_enable(void)
{
	__asm__ volatile(
			"mrc p15, 0, r1, c1, c0, 0\n" /* Read SCTLR (System Control Register) data  */
			"orr r1, r1, #(1 << 17)\n"    /* Enable default map as background region    */
			"orr r1, r1, #(1 << 0)\n"     /* Enable MPU                                 */
			"mcr p15, 0, r1, c1, c0, 0\n" /* Write SCTLR (System Control Register) data */
			"dsb\n"
			"isb\n");
}


static void pmap_mpu_disable(void)
{
	__asm__ volatile(
			"mrc p15, 0, r1, c1, c0, 0\n" /* Read SCTLR (System Control Register) data  */
			"bic r1, r1, #(1 << 0)\n"     /* Disable MPU                                */
			"mcr p15, 0, r1, c1, c0, 0\n" /* Write SCTLR (System Control Register) data */
			"dsb\n"
			"isb\n");
}


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


addr_t pmap_destroy(pmap_t *pmap, unsigned int *i)
{
	return 0;
}


void pmap_switch(pmap_t *pmap)
{
	const hal_syspage_prog_t *hal;
	unsigned int allocCnt;
	spinlock_ctx_t sc;
	unsigned int i;

	if (pmap_common.mpu_enabled == 0) {
		return;
	}

	if (pmap != NULL && pmap->hal != NULL) {
		hal_spinlockSet(&pmap_common.lock, &sc);

		hal = pmap->hal;
		allocCnt = hal->mpu.allocCnt;

		/* Disable MPU */
		pmap_mpu_disable();

		for (i = 0; i < allocCnt; ++i) {
			pmap_mpu_setMemRegionNumber(i);
			pmap_mpu_setMemRegionRbar(hal->mpu.table[i].rbar);
			pmap_mpu_setMemRegionRasr(hal->mpu.table[i].rasr);
		}

		/* Disable all remaining regions */
		for (; i < pmap_common.last_mpu_count[hal_cpuGetID()]; i++) {
			pmap_mpu_setMemRegionNumber(i);
			pmap_mpu_setMemRegionStatus(0);
		}

		/* Enable MPU */
		pmap_mpu_enable();

		pmap_common.last_mpu_count[hal_cpuGetID()] = allocCnt;

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
	const syspage_map_t *map;

	if (pmap_common.mpu_enabled == 0) {
		return 1;
	}

	map = syspage_mapAddrResolve((addr_t)vaddr);
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
	*size = (((size_t)(*top) + SIZE_PAGE - 0x1U) & ~(SIZE_PAGE - 0x1U)) - (size_t)&_end;

	return 0;
}


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	unsigned int cnt = (syspage->hs.mpuType >> 8U) & 0xffU;
	unsigned int i;
	*vstart = (void *)(((ptr_t)&_end + 7U) & ~7U);
	*vend = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */

	pmap->end = (void *)((addr_t)&__bss_start + 32U * 1024U);

	pmap->hal = NULL;
	for (i = 0; i < (unsigned int)NUM_CPUS; i++) {
		pmap_common.last_mpu_count[i] = cnt;
	}

	if (cnt == 0U) {
		hal_spinlockCreate(&pmap_common.lock, "pmap");
		pmap_common.mpu_enabled = 0;
		return;
	}

	pmap_common.mpu_enabled = 1;

	/* Disable MPU that may have been enabled before */
	pmap_mpu_disable();

	for (i = 0; i < cnt; ++i) {
		pmap_mpu_setMemRegionNumber(i);
		pmap_mpu_setMemRegionStatus(0);
	}

	/* Enable MPU */
	pmap_mpu_enable();
	hal_spinlockCreate(&pmap_common.lock, "pmap");
}
