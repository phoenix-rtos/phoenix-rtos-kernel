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
	unsigned int kernelCodeRegion;
	spinlock_t lock;
	int mpu_enabled;
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
	if (pmap_common.mpu_enabled == 0) {
		return 1;
	}

	unsigned int i;
	unsigned int mask = 0U;

	for (i = 0U; i < sizeof(syspage->hs.mpu.map) / sizeof(*syspage->hs.mpu.map); ++i) {
		if (map == syspage->hs.mpu.map[i]) {
			mask |= (1UL << i);
		}
	}

	return mask;
}


int pmap_addMap(pmap_t *pmap, unsigned int map)
{
	if (pmap_common.mpu_enabled == 0) {
		return 0;
	}

	unsigned int rmask = pmap_map2region(map);
	if (rmask == 0U) {
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
			pmap_mpu_setMemRegionNumber(i);

			/* Enable/disable region according to the mask */
			pmap_mpu_setMemRegionStatus(((pmap->regions & (1UL << i)) != 0U) ? 1 : 0);
		}

		hal_spinlockClear(&pmap_common.lock, &sc);
	}
}


int pmap_enter(pmap_t *pmap, addr_t addr, void *vaddr, int attrs, page_t *alloc)
{
	return 0;
}


int pmap_remove(pmap_t *pmap, void *vaddr, void *vend)
{
	return 0;
}


addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	return (addr_t)vaddr;
}


int pmap_isAllowed(pmap_t *pmap, const void *vaddr, size_t size)
{
	const syspage_map_t *map;
	unsigned int rmask;
	if (pmap_common.mpu_enabled == 0) {
		return 1;
	}

	map = syspage_mapAddrResolve((addr_t)vaddr);
	if (map == NULL) {
		return 0;
	}
	rmask = pmap_map2region(map->id);

	return ((pmap->regions & rmask) == 0U) ? 0 : 1;
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


int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top)
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
	const syspage_map_t *ikmap;
	unsigned int ikregion;
	u32 t;
	unsigned int i;
	unsigned int cnt = syspage->hs.mpu.allocCnt;

	*vstart = (void *)(((ptr_t)&_end + 7U) & ~7U);
	*vend = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */

	pmap->end = (void *)((addr_t)&__bss_start + 32U * 1024U);

	pmap->regions = (1UL << cnt) - 1U;

	if (cnt == 0U) {
		hal_spinlockCreate(&pmap_common.lock, "pmap");
		pmap_common.mpu_enabled = 0;
		pmap_common.kernelCodeRegion = 0;
		return;
	}

	pmap_common.mpu_enabled = 1;

	/* Disable MPU that may have been enabled before */
	pmap_mpu_disable();

	for (i = 0; i < cnt; ++i) {
		pmap_mpu_setMemRegionNumber(i);
		t = syspage->hs.mpu.table[i].rbar;
		if ((t & (0x1U << 4)) == 0U) {
			continue;
		}

		pmap_mpu_setMemRegionRbar(t);
		pmap_mpu_setMemRegionRasr(syspage->hs.mpu.table[i].rasr); /* Enable all regions */
	}

	/* Enable MPU */
	pmap_mpu_enable();

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
	if (ikmap == NULL) {
		hal_consolePrint(ATTR_BOLD, "pmap: Kernel code map not found. Bad system config\n");
		for (;;) {
			hal_cpuHalt();
		}
	}

	ikregion = pmap_map2region(ikmap->id);
	if (ikregion == 0U) {
		hal_consolePrint(ATTR_BOLD, "pmap: Kernel code map has no assigned region. Bad system config\n");
		for (;;) {
			hal_cpuHalt();
		}
	}

	pmap_common.kernelCodeRegion = ikregion;

	hal_spinlockCreate(&pmap_common.lock, "pmap");
}
