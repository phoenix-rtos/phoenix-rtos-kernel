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
#include "lib/printf.h"
#include "syspage.h"
#include "halsyspage.h"
#include "vm/kmalloc.h"
#include <arch/cpu.h>
#include <arch/spinlock.h>
#include <lib/lib.h>


#define MPU_BASE ((u32 *)0xe000ed90)


enum {
	mpu_type,
	mpu_ctrl,
	mpu_rnr,
	mpu_rbar,
	mpu_rlar,
	mpu_rbar_a1,
	mpu_rlar_a1,
	mpu_rbar_a2,
	mpu_rlar_a2,
	mpu_rbar_a3,
	mpu_rlar_a3,
	mpu_mair0 = 0xC,
	mpu_mair1
};


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

char b[128];


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, syspage_prog_t *prog, void *vaddr)
{
	hal_memcpy(&pmap->hal, &prog->hal, sizeof(hal_syspage_prog_t));
	pmap->regions = pmap_common.kernelCodeRegion;

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

#ifdef MPUTEST_ORGIMPL

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
#ifndef MPUTEST_ORGIMPL_OPT
			hal_cpuDataMemoryBarrier();
#endif

			/* Enable/disable region according to the mask */
			if ((pmap->regions & (1 << i)) != 0) {
				*(pmap_common.mpu + mpu_rlar) |= 1u;
			}
			else {
				*(pmap_common.mpu + mpu_rlar) &= ~1u;
			}
#ifndef MPUTEST_ORGIMPL_OPT
			hal_cpuDataMemoryBarrier();
#endif
		}
#ifdef MPUTEST_ORGIMPL_OPT
		hal_cpuDataMemoryBarrier();
#endif
		hal_spinlockClear(&pmap_common.lock, &sc);
	}
}

#else /* MPUTEST_ORGIMPL */

void pmap_switch(pmap_t *pmap)
{
	unsigned int i;
	spinlock_ctx_t sc;

	if (pmap_common.mpu_enabled == 0) {
		return;
	}

	if (pmap != NULL) {
		hal_spinlockSet(&pmap_common.lock, &sc);

		/* Disable MPU */
		hal_cpuDataMemoryBarrier();
		*(pmap_common.mpu + mpu_ctrl) &= ~1;


#ifdef MPUTEST_ASMOPT

		void *tmp_table = pmap->hal.table;
		i = 0;
		__asm__ volatile(
				"cmp %[i], %[cnt]                      \n\t" /* Check if there's anything to do */
				"bge 2f                                \n\t" /* Exit if not */
				"1:                                    \n\t" /* Loop body */
				"str %[i], [%[mpu_rnr]]                \n\t" /* MPU_RNR = i */
				"ldmia %[tmp_table]!, {r4-r11}         \n\t" /* Load 4 regions (rbar/rlar pairs) from table, update table pointer */
				"stmia %[mpu_rbar], {r4-r11}           \n\t" /* Write 4 regions via RBAR/RLAR and aliases */
				"add %[i], %[i], #4                    \n\t" /* i += 4 */
				"cmp %[i], %[cnt]                      \n\t"
				"blt 1b                                \n\t" /* Branch if i < allocCnt */
				"2:                                    \n\t"
				: [tmp_table] "+r"(tmp_table),
				[i] "+r"(i)
				: [cnt] "r"(max(pmap->hal.allocCnt, pmap_common.last_mpu_count)),
				[mpu_rbar] "r"(MPU_BASE + mpu_rbar),
				[mpu_rnr] "r"(MPU_BASE + mpu_rnr)
				: "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "cc", "memory");
#else

		for (i = 0; i < pmap->hal.allocCnt; ++i) {
			/* Select region */
			*(pmap_common.mpu + mpu_rnr) = i;

			*(pmap_common.mpu + mpu_rbar) = pmap->hal.table[i].rbar;
			*(pmap_common.mpu + mpu_rlar) = pmap->hal.table[i].rlar;
		}

		/* Disable all remaining regions */
		for (; i < pmap_common.last_mpu_count; i++) {
			/* Select region */
			*(pmap_common.mpu + mpu_rnr) = i;
			*(pmap_common.mpu + mpu_rlar) = 0;
		}

#endif
		pmap_common.last_mpu_count = pmap->hal.allocCnt;


		/* Enable MPU */
		*(pmap_common.mpu + mpu_ctrl) |= 1;
		hal_cpuDataSyncBarrier();

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


#ifdef MPUTEST_ORGIMPL
	unsigned int rmask = pmap_map2region(map->id);

	return ((pmap->regions & rmask) != 0) ? 1 : 0;
#else
	for (int i = 0; i < sizeof(pmap->hal.map) / sizeof(pmap->hal.map[0]); ++i) {
		if (pmap->hal.map[i] == map->id) {
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
pmap_t maps[4];

void pmap_switch_bulktest(int cacheopt)
{
	int ITER_CNT = 10 * 1000;

	spinlock_t tmpLock;
	spinlock_ctx_t sc;
	hal_spinlockCreate(&tmpLock, "pmap_bulk_test");
	hal_spinlockSet(&tmpLock, &sc);  // prevent context switch during test

	unsigned int orgCnt = syspage->hs.mpu.allocCnt;

	int cpuCycles;

	lib_sprintf(b, "Starting pmap_switch bulk test with cacheopt=%d\n", cacheopt);
	hal_consolePrint(ATTR_BOLD, b);

	if (cacheopt == 0) {
		_hal_scsICacheDisable();
		_hal_scsDCacheDisable();
	}
	else {
		_hal_scsICacheEnable();
		_hal_scsDCacheEnable();
	}

	testGPIOlatencyConfigure();

	for (int allocCnt = 4; allocCnt <= (u8)(syspage->hs.mpu.type >> 8); allocCnt += 4) {
		/* Prepare test pmaps */
		for (int i = 0; i < 4; i++) {
			// maps[i].hal = vm_kmalloc(sizeof(hal_syspage_prog_t));
			maps[i].hal.allocCnt = allocCnt;
			for (int r = 0; r < allocCnt; r++) {
				maps[i].hal.table[r].rbar =
						(r * 0x1000 + 0x20000000) | /* base address, in ram */
						(0x13);                     /* rw, shareable, no exec*/
				maps[i].hal.table[r].rlar =
						((r + 1) * 0x1000 + 0x20000000) | /* limit address, in ram */
						(0x17);                           /* cacheable, bufferable, exec never, enabled*/
			}
			for (int r = allocCnt; r < (u8)(syspage->hs.mpu.type >> 8); r++) {
				maps[i].hal.table[r].rbar = 0;  // disabled
				maps[i].hal.table[r].rlar = 0;
			}
			for (int j = 0; j < allocCnt; j += 4) {
				// just to differentiate
				maps[i].regions = 1 << (i + j);
			}
		}
		syspage->hs.mpu.allocCnt = allocCnt; /* to keep orgimpl working */

		/* TEST MPU SWITCH TIME */
		cpuCycles = hal_timerGetCyc();
		for (int iter = 0; iter < ITER_CNT; iter++) {
			pmap_switch(&maps[iter % 4]);
		}
		cpuCycles = hal_timerGetCyc() - cpuCycles;

		MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
		for (int iter = 0; iter < ITER_CNT; iter++) {
			MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
			pmap_switch(&maps[iter % 4]);
			MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);
			for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 20;) {
				__asm__ volatile("nop");
			}
		}
		MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);

		hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
		lib_sprintf(b, "pmap_switch() with %d regions - %d times: %d cycles (%d us)\n",
				allocCnt, ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
		hal_consolePrint(ATTR_USER, b);


		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 2000;) {
			__asm__ volatile("nop");
		}

#ifdef MPUTEST_FIXED
		/* TEST MPU REGION DISABLE TIME */
		cpuCycles = hal_timerGetCyc();
		for (int iter = 0; iter < ITER_CNT; iter++) {
			*(pmap_common.mpu + mpu_rnr) = (iter % allocCnt);
			*(pmap_common.mpu + mpu_rlar) |= 1;
		}
		cpuCycles = hal_timerGetCyc() - cpuCycles;

		MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
		for (int iter = 0; iter < ITER_CNT; iter++) {
			MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
			*(pmap_common.mpu + mpu_rnr) = (iter % allocCnt);
			*(pmap_common.mpu + mpu_rlar) |= 1;
			MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);

			for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 20;) {
				__asm__ volatile("nop");
			}
		}
		MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);

		hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
		lib_sprintf(b, "region disable/enable with %d regions - %d times: %d cycles (%d us)\n",
				allocCnt, ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
		hal_consolePrint(ATTR_USER, b);

		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 2000;) {
			__asm__ volatile("nop");
		}
#endif
	}


#ifdef MPUTEST_FIXED
	/* TEST MPU DISABLE TIME */
	cpuCycles = hal_timerGetCyc();
	for (int iter = 0; iter < ITER_CNT; iter++) {
		*(pmap_common.mpu + mpu_ctrl) &= ~1;
		*(pmap_common.mpu + mpu_ctrl) |= 1;
	}
	cpuCycles = hal_timerGetCyc() - cpuCycles;
	MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
	for (int iter = 0; iter < ITER_CNT; iter++) {
		MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
		*(pmap_common.mpu + mpu_ctrl) &= ~1;
		*(pmap_common.mpu + mpu_ctrl) |= 1;
		MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);
		for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 20;) {
			__asm__ volatile("nop");
		}
	}
	MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);

	hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
	lib_sprintf(b, "MPU on/off - %d times: %d cycles (%d us)\n",
			ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
	hal_consolePrint(ATTR_USER, b);


	for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 2000;) {
		__asm__ volatile("nop");
	}

	testGPIOlatency(cacheopt);

	// /* TESTs for cache maintenance */
	cpuCycles = hal_timerGetCyc();
	MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
	for (int i = 0; i < ITER_CNT; i++) {
		MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
		_hal_scsDCacheDisable();
		MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);
		_hal_scsDCacheEnable();
	}
	MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);
	cpuCycles = hal_timerGetCyc() - cpuCycles;
	hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
	lib_sprintf(b, "DCache DISABLE/enable - %d times: %d cycles (%d us)\n",
			ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
	hal_consolePrint(ATTR_USER, b);

	for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 2000;) {
		__asm__ volatile("nop");
	}

	cpuCycles = hal_timerGetCyc();
	MPUTEST_GPIO_SET(MPUTEST_PORT0, MPUTEST_PIN0);
	for (int i = 0; i < ITER_CNT; i++) {
		MPUTEST_GPIO_SET(MPUTEST_PORT1, MPUTEST_PIN1);
		_hal_scsICacheDisable();
		MPUTEST_GPIO_CLR(MPUTEST_PORT1, MPUTEST_PIN1);
		_hal_scsICacheEnable();
	}
	MPUTEST_GPIO_CLR(MPUTEST_PORT0, MPUTEST_PIN0);
	cpuCycles = hal_timerGetCyc() - cpuCycles;
	hal_consolePrint(ATTR_BOLD, "--------------------------------------------------\n");
	lib_sprintf(b, "ICache DISABLE/enable - %d times: %d cycles (%d us)\n",
			ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
	hal_consolePrint(ATTR_USER, b);

	for (int i = hal_timerGetCyc(); hal_timerGetCyc() - i < 2000;) {
		__asm__ volatile("nop");
	}
#endif

	_hal_scsICacheEnable();
	_hal_scsDCacheEnable();

	syspage->hs.mpu.allocCnt = orgCnt;
#ifdef MPUTEST_ORGIMPL
	for (int i = 0; i < (u8)(syspage->hs.mpu.type >> 8); ++i) {
		/* Select MPU region to configure */
		*(pmap_common.mpu + mpu_rnr) = i;
		hal_cpuDataMemoryBarrier();

		*(pmap_common.mpu + mpu_rbar) = syspage->hs.mpu.table[i].rbar;
		hal_cpuDataMemoryBarrier();

		/* Disable regions for now */
		*(pmap_common.mpu + mpu_rlar) = syspage->hs.mpu.table[i].rlar & ~1u;
		hal_cpuDataMemoryBarrier();
	}

	/* Enable MPU */
	*(pmap_common.mpu + mpu_ctrl) |= 1;
	hal_cpuDataMemoryBarrier();
#else

	/* Disable MPU, will come back after pmap_switch */
	*(pmap_common.mpu + mpu_ctrl) &= ~1;
	hal_cpuDataMemoryBarrier();
	pmap_common.last_mpu_count = (syspage->hs.mpu.type >> 8) & 0xff;
#endif

	hal_spinlockClear(&tmpLock, &sc);
	hal_spinlockDestroy(&tmpLock);
}


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	const syspage_map_t *ikmap;
	unsigned int ikregion;
	unsigned int i, cnt = (syspage->hs.mpu.type >> 8) & 0xff;

	(*vstart) = (void *)(((ptr_t)_init_vectors + 7) & ~7u);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */
	pmap->end = (void *)((addr_t)&__bss_start + 32 * 1024);

	/* Enable all regions for kernel */
	pmap->regions = (1 << cnt) - 1;

	pmap->hal.allocCnt = 0; /* TODO: temporary to skip kernel mpu reconfiguration */

	/* Configure MPU */
	pmap_common.mpu = MPU_BASE;

	hal_spinlockCreate(&pmap_common.lock, "pmap");
	if (cnt == 0) {
		pmap_common.mpu_enabled = 0;
		pmap_common.kernelCodeRegion = 0;
		return;
	}

	pmap_common.last_mpu_count = (syspage->hs.mpu.type >> 8) & 0xff;

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
		*(pmap_common.mpu + mpu_rlar) = syspage->hs.mpu.table[i].rlar & ~1u;
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
