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
#include "hal/arm/scs.h"
#include "hal/console.h"
#include "hal/spinlock.h"
#include "include/syspage.h"
#include "syspage.h"
#include "halsyspage.h"
#include <arch/cpu.h>
#include <arch/spinlock.h>
#include "lib/lib.h"


/* Linker symbols */
extern unsigned int _end;
extern unsigned int __bss_start;

u8 _init_stack[NUM_CPUS][SIZE_INITIAL_KSTACK] __attribute__((aligned(8)));


static struct {
	volatile u32 *mpu;
	unsigned int kernelCodeRegion;
	spinlock_t lock;
	int last_mpu_count;
} pmap_common;
// static char b[200];


#define MPU_LOC ((u32 *)0xe000ed90)


static void pmap_mpu_setMemRegionNumber(u32 num)
{
	__asm__ volatile("mcr p15, 0, %0, c6, c2, 0" ::"r"(num));
}


static void pmap_mpu_setMemRegionRasr(u32 rasr)
{
	/* ARMv7-R uses the same region attribute bits as ARMv7-M, but they are split over two registers */
	u32 rser = rasr & 0xffff;
	u32 racr = rasr >> 16;
	__asm__ volatile("mcr p15, 0, %0, c6, c1, 2" ::"r"(rser));
	__asm__ volatile("mcr p15, 0, %0, c6, c1, 4" ::"r"(racr));
}


static void pmap_mpu_setMemRegionStatus(int enable)
{
	u32 val;
	__asm__ volatile("mrc p15, 0, %0, c6, c1, 2" : "=r"(val));
	if (enable != 0) {
		val |= 1;
	}
	else {
		val &= ~1;
	}

	__asm__ volatile("mcr p15, 0, %0, c6, c1, 2" ::"r"(val));
}


static void pmap_mpu_setMemRegionRbar(u32 addr)
{
	addr &= ~((1 << 5) - 1);
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
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, syspage_prog_t *prog, void *vaddr)
{
	pmap->prog = prog;
	return 0;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	return 0;
}


static unsigned int pmap_map2region(unsigned int map)
{
#ifndef MPUTEST_ORGIMPL
	return 0xffff;
#else
	int i;
	unsigned int mask = 0;

	for (i = 0; i < sizeof(syspage->hs.mpu.map) / sizeof(*syspage->hs.mpu.map); ++i) {
		if (map == syspage->hs.mpu.map[i]) {
			mask |= (1 << i);
		}
	}

	return mask;
#endif
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


#ifdef MPUTEST_ORGIMPL

void pmap_switch(pmap_t *pmap)
{
	unsigned int i, cnt = syspage->hs.mpu.allocCnt;
	spinlock_ctx_t sc;

	if (pmap != NULL) {
		hal_spinlockSet(&pmap_common.lock, &sc);
		for (i = 0; i < cnt; ++i) {
			/* Select region */
			pmap_mpu_setMemRegionNumber(i);

			/* Enable/disable region according to the mask */
			pmap_mpu_setMemRegionStatus((pmap->regions & (1 << i)) != 0);
		}
		hal_spinlockClear(&pmap_common.lock, &sc);
	}
}

#else /* MPUTEST_ORGIMPL */

void pmap_switch(pmap_t *pmap)
{
	unsigned int i;
	spinlock_ctx_t sc;

	if (pmap != NULL) {
		hal_spinlockSet(&pmap_common.lock, &sc);

		/* Disable MPU */
		pmap_mpu_disable();


#ifdef MPUTEST_ASMOPT
#error "ASMOPT not yet supported by v7r: could use precomputations and single store for triplet of registers"
#else /* MPUTEST_ASMOPT */

#ifdef MPUTEST_FORCEALL
		for (i = 0; i < (u8)(syspage->hs.mpu.type >> 8); ++i) {
#else
		for (i = 0; i < pmap->prog->hal.allocCnt; ++i) {
#endif
			pmap_mpu_setMemRegionNumber(i);
			pmap_mpu_setMemRegionRbar(pmap->prog->hal.table[i].rbar);
			pmap_mpu_setMemRegionRasr(pmap->prog->hal.table[i].rasr);
		}

#if !defined(MPUTEST_FORCEALL)

		// #MPUTEST: with MPUTEST_ASMOPT opt here is omitted because most pmap will have similar allocCnt,
		// and i is incremented by 4 in asm loop so this shouldn't have large impact

		/* Disable all remaining regions */
		for (; i < pmap_common.last_mpu_count; i++) {
			pmap_mpu_setMemRegionNumber(i);
			pmap_mpu_setMemRegionStatus(0);
		}
		pmap_common.last_mpu_count = pmap->prog->hal.allocCnt;
#endif

#endif /* MPUTEST_ASMOPT */

		/* Enable MPU */
		pmap_mpu_enable();

		hal_spinlockClear(&pmap_common.lock, &sc);
	}
}

#endif /* MPUTEST_ORGIMPL */

int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc)
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
	if (map == NULL) {
		return 0;
	}
#ifdef MPUTEST_ORGIMPL
	unsigned int rmask = pmap_map2region(map->id);
	return ((pmap->regions & rmask) == 0) ? 0 : 1;
#else
	for (int i = 0; i < sizeof(pmap->prog->hal.map) / sizeof(pmap->prog->hal.map[0]); ++i) {
		if (pmap->prog->hal.map[i] == map->id) {
			return 1;
		}
	}
	return 0;
#endif
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
	if (i != 0) {
		return -1;
	}

	/* Returns region above basic kernel's .bss section */
	*vaddr = (void *)&_end;
	*size = (((size_t)(*top) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1)) - (size_t)&_end;

	return 0;
}

extern time_t hal_timerCyc2Us(time_t ticks);
extern time_t hal_timerGetCyc(void);

// #MPUTEST: Bulk test of pmap_switch performance

void pmap_switch_bulktest(int cacheopt)
{
	// 	pmap_t maps[4];
	// 	int ITER_CNT = 10 * 1000;

	// 	spinlock_t tmpLock;
	// 	spinlock_ctx_t sc;
	// 	hal_spinlockCreate(&tmpLock, "pmap_bulk_test");
	// 	hal_spinlockSet(&tmpLock, &sc);  // prevent context switch during test

	// 	unsigned int orgCnt = syspage->hs.mpu.allocCnt;

	// 	int cpuCycles;

	// 	if (cacheopt == 0) {
	// 		_hal_scsICacheDisable();
	// 		_hal_scsDCacheDisable();
	// 	}
	// 	else {
	// 		_hal_scsICacheEnable();
	// 		_hal_scsDCacheEnable();
	// 	}

	// 	testGPIOlatencyConfigure();

	// 	for (int allocCnt = 4; allocCnt <= (u8)(syspage->hs.mpu.type >> 8); allocCnt += 4) {
	// 		/* Prepare test pmaps */
	// 		for (int i = 0; i < 4; i++) {
	// 			maps[i].mpu.allocCnt = allocCnt;
	// 			for (int r = 0; r < allocCnt; r++) {
	// 				maps[i].mpu.table[r].rbar =
	// 						(r * 0x1000 + 0x40000) | /* base address, outside of itcm */
	// 						(1u << 4) |
	// 						(r & 0xfu);
	// 				u32 attr = (((0) << 28) |          /* execute never */
	// 						(((r % 4) & 0x7u) << 24) | /* access permissions */
	// 						((0 & 0x7u) << 19) |       /* tex */
	// 						(((1) & 1u) << 18) |       /* s */
	// 						(((1) & 1u) << 17) |       /* c */
	// 						(((1) & 1u) << 16) |       /* b */
	// 						(1));                      /* enable */
	// 				maps[i].mpu.table[r].rasr =
	// 						attr |
	// 						(((u32)r) << 8) |            /* subregions */
	// 						((((u32)0xb) & 0x1fu) << 1); /* region size = 4KB */
	// 			}
	// 			for (int r = allocCnt; r < (u8)(syspage->hs.mpu.type >> 8); r++) {
	// 				maps[i].mpu.table[r].rbar = 0;  // disabled
	// 				maps[i].mpu.table[r].rasr = 0;
	// 			}
	// 			for (int j = 0; j < allocCnt; j += 4) {
	// 				// just to differentiate
	// 				maps[i].regions = 1 << (i + j);
	// 			}
	// 		}
	// 		syspage->hs.mpu.allocCnt = allocCnt;

	// 		/* TEST MPU SWITCH TIME */
	// 		cpuCycles = hal_timerGetCyc();
	// 		for (int iter = 0; iter < ITER_CNT; iter++) {
	// 			pmap_switch(&maps[iter % 4]);
	// 		}
	// 		cpuCycles = hal_timerGetCyc() - cpuCycles;

	// 		MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
	// 		for (int iter = 0; iter < ITER_CNT; iter++) {
	// 			MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
	// 			pmap_switch(&maps[iter % 4]);
	// 			MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);
	// 			for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 1000;) {
	// 				__asm__ volatile("nop");
	// 			}
	// 			if (cacheopt == 1) {
	// 				hal_invalDCacheAll();
	// 				hal_invalICacheAll();
	// 			}
	// 		}
	// 		MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);

	// 		hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
	// 		lib_sprintf(b, "pmap_switch() with %d regions - %d times: %d cycles (%d us)\n",
	// 				allocCnt, ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
	// 		hal_consolePrint(ATTR_USER, b);


	// 		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100000;) {
	// 			__asm__ volatile("nop");
	// 		}

	// #ifdef MPUTEST_FIXED
	// 		/* TEST MPU REGION DISABLE TIME */
	// 		cpuCycles = hal_timerGetCyc();
	// 		for (int iter = 0; iter < ITER_CNT; iter++) {
	// 			*(pmap_common.mpu + mpu_rnr) = (iter % allocCnt);
	// 			*(pmap_common.mpu + mpu_rasr) |= 1;
	// 		}
	// 		cpuCycles = hal_timerGetCyc() - cpuCycles;

	// 		MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
	// 		for (int iter = 0; iter < ITER_CNT; iter++) {
	// 			MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
	// 			*(pmap_common.mpu + mpu_rnr) = (iter % allocCnt);
	// 			*(pmap_common.mpu + mpu_rasr) |= 1;
	// 			MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);

	// 			for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 1000;) {
	// 				__asm__ volatile("nop");
	// 			}
	// 			if (cacheopt == 1) {
	// 				hal_invalDCacheAll();
	// 				hal_invalICacheAll();
	// 			}
	// 		}
	// 		MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);

	// 		hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
	// 		lib_sprintf(b, "region disable/enable with %d regions - %d times: %d cycles (%d us)\n",
	// 				allocCnt, ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
	// 		hal_consolePrint(ATTR_USER, b);

	// 		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100000;) {
	// 			__asm__ volatile("nop");
	// 		}
	// #endif
	// 	}


	// #ifdef MPUTEST_FIXED
	// 	/* TEST MPU DISABLE TIME */
	// 	cpuCycles = hal_timerGetCyc();
	// 	for (int iter = 0; iter < ITER_CNT; iter++) {
	// 		*(pmap_common.mpu + mpu_ctrl) &= ~1;
	// 		*(pmap_common.mpu + mpu_ctrl) |= 1;
	// 	}
	// 	cpuCycles = hal_timerGetCyc() - cpuCycles;
	// 	MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
	// 	for (int iter = 0; iter < ITER_CNT; iter++) {
	// 		MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
	// 		*(pmap_common.mpu + mpu_ctrl) &= ~1;
	// 		*(pmap_common.mpu + mpu_ctrl) |= 1;
	// 		MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);
	// 		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 1000;) {
	// 			__asm__ volatile("nop");
	// 		}
	// 		if (cacheopt == 1) {
	// 			hal_invalDCacheAll();
	// 			hal_invalICacheAll();
	// 		}
	// 	}
	// 	MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);

	// 	hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
	// 	lib_sprintf(b, "MPU on/off - %d times: %d cycles (%d us)\n",
	// 			ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
	// 	hal_consolePrint(ATTR_USER, b);


	// 	for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100000;) {
	// 		__asm__ volatile("nop");
	// 	}

	// 	testGPIOlatency(cacheopt);

	// 	// /* TESTs for cache maintenance */
	// 	cpuCycles = hal_timerGetCyc();
	// 	MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
	// 	for (int i = 0; i < ITER_CNT; i++) {
	// 		MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
	// 		_hal_scsDCacheDisable();
	// 		MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);
	// 		_hal_scsDCacheEnable();
	// 		if (cacheopt == 1) {
	// 			hal_invalDCacheAll();
	// 			hal_invalICacheAll();
	// 		}
	// 	}
	// 	MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);
	// 	cpuCycles = hal_timerGetCyc() - cpuCycles;
	// 	hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
	// 	lib_sprintf(b, "DCache DISABLE/enable - %d times: %d cycles (%d us)\n",
	// 			ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
	// 	hal_consolePrint(ATTR_USER, b);

	// 	for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100000;) {
	// 		__asm__ volatile("nop");
	// 	}

	// 	cpuCycles = hal_timerGetCyc();
	// 	MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
	// 	for (int i = 0; i < ITER_CNT; i++) {
	// 		MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
	// 		_hal_scsICacheDisable();
	// 		MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);
	// 		_hal_scsICacheEnable();
	// 		if (cacheopt == 1) {
	// 			hal_invalDCacheAll();
	// 			hal_invalICacheAll();
	// 		}
	// 	}
	// 	MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);
	// 	cpuCycles = hal_timerGetCyc() - cpuCycles;
	// 	hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
	// 	lib_sprintf(b, "ICache DISABLE/enable - %d times: %d cycles (%d us)\n",
	// 			ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
	// 	hal_consolePrint(ATTR_USER, b);

	// 	for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 100000;) {
	// 		__asm__ volatile("nop");
	// 	}
	// #endif

	// 	_hal_scsICacheEnable();
	// 	_hal_scsDCacheEnable();

	// 	syspage->hs.mpu.allocCnt = orgCnt;
	// #ifdef MPUTEST_ORGIMPL
	// 	u32 t;
	// 	for (int i = 0; i < syspage->hs.mpu.allocCnt; ++i) {
	// 		t = syspage->hs.mpu.table[i].rbar;
	// 		if ((t & (1 << 4)) == 0) {
	// 			continue;
	// 		}

	// 		*(pmap_common.mpu + mpu_rbar) = t;
	// 		hal_cpuDataMemoryBarrier();

	// 		/* Disable regions for now */
	// 		t = syspage->hs.mpu.table[i].rasr & ~1;
	// 		*(pmap_common.mpu + mpu_rasr) = t;
	// 		hal_cpuDataMemoryBarrier();
	// 	}
	// #endif

	// 	hal_spinlockClear(&tmpLock, &sc);
	// 	hal_spinlockDestroy(&tmpLock);
}

void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	const syspage_map_t *ikmap;
	unsigned int ikregion;
	// u32 t;
	unsigned int cnt = syspage->hs.mpu.allocCnt;

	(*vstart) = (void *)(((ptr_t)&_end + 7) & ~7u);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */
	pmap->end = (void *)((addr_t)&__bss_start + 32 * 1024);

	pmap->regions = (1 << cnt) - 1;
	pmap->prog->hal.allocCnt = 0; /* TODO: temporary to skip kernel mpu reconfiguration */


	/* Disable MPU that may have been enabled before */
	// pmap_mpu_disable();

	// for (i = 0; i < cnt; ++i) {
	// 	pmap_mpu_setMemRegionNumber(i);
	// 	t = syspage->hs.mpu.table[i].rbar;
	// 	if ((t & (1 << 4)) == 0) {
	// 		continue;
	// 	}

	// 	pmap_mpu_setMemRegionRbar(t);
	// 	pmap_mpu_setMemRegionRasr(syspage->hs.mpu.table[i].rasr);
	// 	pmap_mpu_setMemRegionStatus(0);
	// }

	// /* Enable MPU */
	// pmap_mpu_enable();

	/* FIXME HACK
	 * allow all programs to execute (and read) kernel code map.
	 * Needed because of hal_jmp, syscalls handler and signals handler.
	 * In these functions we need to switch to the user mode when still
	 * executing kernel code. This will cause memory management fault
	 * if the application does not have access to the kernel instruction
	 * map. Possible fix - place return to the user code in the separate
	 * region and allow this region instead. */

	/* Find kernel code region */
	ikmap = syspage_mapAddrResolve((addr_t)(void *)_pmap_init);
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

	hal_spinlockCreate(&pmap_common.lock, "pmap");

	pmap_common.last_mpu_count = syspage->hs.mpu.type >> 8;
}
