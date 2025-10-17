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
#include <arch/spinlock.h>
#include "lib/lib.h"

enum { mpu_type,
	mpu_ctrl,
	mpu_rnr,
	mpu_rbar,
	mpu_rasr,
	mpu_rbar_a1,
	mpu_rasr_a1,
	mpu_rbar_a2,
	mpu_rasr_a2,
	mpu_rbar_a3,
	mpu_rasr_a3 };

/* Linker symbols */
extern unsigned int _end;
extern unsigned int __bss_start;

extern void *_init_vectors;

static struct {
	volatile u32 *mpu;
	unsigned int kernelCodeRegion;
	spinlock_t lock;
	int last_mpu_count;
} pmap_common;


#define MPU_LOC ((u32 *)0xe000ed90)


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	pmap->regions = pmap_common.kernelCodeRegion;

	// TODO: temporary hack!
	if (vaddr != NULL) {
		hal_memcpy(&pmap->mpu, vaddr, sizeof(pmap->mpu));
	}


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
		hal_cpuDataMemoryBarrier();
		*(pmap_common.mpu + mpu_ctrl) &= ~1;


#ifdef MPUTEST_ASMOPT
		/* The following assembly block replaces the original C loop for MPU region programming.
		 * It is optimized to program up to 4 MPU regions per iteration using aliased registers
		 * and the `stmia` instruction, which is more efficient than programming one region at a time.
		 *
		 * A single data memory barrier is issued after each `stmia` to ensure MPU register
		 * writes complete before proceeding. This is required by the ARMv7-M architecture.
		 *
		 * The loop processes regions in chunks of 4. Any remaining regions (1 to 3) are
		 * handled individually after the main loop.
		 *
		 * Clobbered registers are declared to inform the compiler about their usage.
		 */
		void *tmp_table = pmap->mpu.table;
		i = 0;
		__asm__ volatile(
				"cmp %[i], %[cnt]                      \n\t" /* Check if there's anything to do */
				"bge 2f                                \n\t" /* Exit if not */
				"1:                                    \n\t" /* Loop body */
				"str %[i], [%[mpu_rnr]]                \n\t" /* MPU_RNR = i */
				"ldmia %[tmp_table]!, {r4-r11}         \n\t" /* Load 4 regions (rbar/rasr pairs) from table, update table pointer */
				"stmia %[mpu_rbar], {r4-r11}           \n\t" /* Write 4 regions via RBAR/RASR and aliases */
				"add %[i], %[i], #4                    \n\t" /* i += 4 */
				"cmp %[i], %[cnt]                      \n\t"
				"blt 1b                                \n\t" /* Branch if i < allocCnt */
				"2:                                    \n\t"
				: [tmp_table] "+r"(tmp_table),
				[i] "+r"(i)
				: [cnt] "r"(pmap->mpu.allocCnt),
				[mpu_rbar] "r"(pmap_common.mpu + mpu_rbar),
				[mpu_rnr] "r"(pmap_common.mpu + mpu_rnr)
				: "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "cc", "memory");
#else /* MPUTEST_ASMOPT */

		u32 t;

#ifdef MPUTEST_FORCEALL
		for (i = 0; i < (u8)(syspage->hs.mpu.type >> 8); ++i) {
#else
		for (i = 0; i < pmap->mpu.allocCnt; ++i) {
#endif
			/* Select region */
			*(pmap_common.mpu + mpu_rnr) = i;

			t = pmap->mpu.table[i].rbar;
			if ((t & (1 << 4)) == 0) {
				hal_consolePrint(ATTR_BOLD, "pmap: Trying to enable disabled region\n");
				continue;
			}

			*(pmap_common.mpu + mpu_rbar) = t;

			*(pmap_common.mpu + mpu_rasr) = pmap->mpu.table[i].rasr;
		}

#if !defined(MPUTEST_FORCEALL)

		// #MPUTEST: with MPUTEST_ASMOPT opt here is omitted because most pmap will have similar allocCnt,
		// and i is incremented by 4 in asm loop so this shouldn't have large impact

		/* Disable all remaining regions */
		for (; i < pmap_common.last_mpu_count; i++) {
			/* Select region */
			*(pmap_common.mpu + mpu_rnr) = i;
			*(pmap_common.mpu + mpu_rasr) = 0;
		}
		pmap_common.last_mpu_count = pmap->mpu.allocCnt;
#endif

#endif /* MPUTEST_ASMOPT */

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
	if (map == NULL) {
		return 0;
	}
#ifdef MPUTEST_ORGIMPL
	unsigned int rmask = pmap_map2region(map->id);
	return ((pmap->regions & rmask) == 0) ? 0 : 1;
#else
	for (int i = 0; i < sizeof(pmap->mpu.map) / sizeof(pmap->mpu.map[0]); ++i) {
		if (pmap->mpu.map[i] == map->id) {
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
	if (i)
		return -1;

	/* Returns region above basic kernel's .bss section */
	*vaddr = (void *)&_end;
	*size = (((size_t)(*top) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1)) - (size_t)&_end;

	return 0;
}

extern time_t hal_timerCyc2Us(time_t ticks);
extern time_t hal_timerGetCyc(void);

static void pmap_switch_bulktest(void)  // #MPUTEST: Bulk test of pmap_switch performance
{
	pmap_t maps[4];
	int ITER_CNT = 10 * 1000;

	/* TEST MPU DISABLE TIME */
	int cpuCycles = hal_timerGetCyc();
	*(GPIO9_BASE + dr_set) = 1u << MPUTEST_PIN0;
	for (int iter = 0; iter < ITER_CNT; iter++) {
		*(GPIO9_BASE + dr_set) = 1u << MPUTEST_PIN1;
		*(pmap_common.mpu + mpu_ctrl) &= ~1;
		*(pmap_common.mpu + mpu_ctrl) |= 1;
		*(GPIO9_BASE + dr_clr) = 1u << MPUTEST_PIN1;
	}
	*(GPIO9_BASE + dr_clr) = 1u << MPUTEST_PIN0;
	cpuCycles = hal_timerGetCyc() - cpuCycles;

	char b[200];
	hal_consolePrint(ATTR_BOLD, "--------------------------------------------------");
	lib_sprintf(b, "MPU on/off - %d times: %d cycles (%d us)",
			ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
	hal_consolePrint(ATTR_BOLD, b);


	for (int i = 0; i < ITER_CNT; i++) {
		__asm__ volatile("nop");
	}


	for (int allocCnt = 4; allocCnt <= (u8)(syspage->hs.mpu.type >> 8); allocCnt += 4) {
		/* Prepare test pmaps */
		for (int i = 0; i < 4; i++) {
			maps[i].mpu.allocCnt = allocCnt;
			for (int r = 0; r < allocCnt; r++) {
				maps[i].mpu.table[r].rbar =
						(r * 0x1000) | /* base address */
						(1u << 4) |
						(r & 0xfu);
				u32 attr = (((0) << 28) |          /* execute never */
						(((r % 4) & 0x7u) << 24) | /* access permissions */
						((0 & 0x7u) << 19) |       /* tex */
						(((1) & 1u) << 18) |       /* s */
						(((1) & 1u) << 17) |       /* c */
						(((1) & 1u) << 16) |       /* b */
						(1));                      /* enable */
				maps[i].mpu.table[r].rasr =
						attr |
						(((u32)r) << 8) |            /* subregions */
						((((u32)0xb) & 0x1fu) << 1); /* region size = 4KB */
			}
			for (int r = allocCnt; r < (u8)(syspage->hs.mpu.type >> 8); r++) {
				maps[i].mpu.table[r].rbar = 0;  // disabled
				maps[i].mpu.table[r].rasr = 0;
			}
			for (int j = 0; j < allocCnt; j += 4) {
				// just to differentiate
				maps[i].regions = 1 << (i + j);
			}
		}

		/* TEST MPU SWITCH TIME */
		cpuCycles = hal_timerGetCyc();
		*(GPIO9_BASE + dr_set) = 1u << MPUTEST_PIN0;
		for (int iter = 0; iter < ITER_CNT; iter++) {
			*(GPIO9_BASE + dr_set) = 1u << MPUTEST_PIN1;
			pmap_switch(&maps[iter % 4]);
			*(GPIO9_BASE + dr_clr) = 1u << MPUTEST_PIN1;
		}
		*(GPIO9_BASE + dr_clr) = 1u << MPUTEST_PIN0;
		cpuCycles = hal_timerGetCyc() - cpuCycles;


		hal_consolePrint(ATTR_BOLD, "--------------------------------------------------");
		lib_sprintf(b, "pmap_switch() with %d regions - %d times: %d cycles (%d us)",
				allocCnt, ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
		hal_consolePrint(ATTR_BOLD, b);


		for (int i = 0; i < ITER_CNT; i++) {
			__asm__ volatile("nop");
		}

		/* TEST MPU REGION DISABLE TIME */
		cpuCycles = hal_timerGetCyc();
		*(GPIO9_BASE + dr_set) = 1u << MPUTEST_PIN0;
		for (int iter = 0; iter < ITER_CNT; iter++) {
			*(GPIO9_BASE + dr_set) = 1u << MPUTEST_PIN1;
			*(pmap_common.mpu + mpu_rnr) = (iter % allocCnt);
			*(pmap_common.mpu + mpu_rasr) &= ~1;
			*(GPIO9_BASE + dr_clr) = 1u << MPUTEST_PIN1;

			*(pmap_common.mpu + mpu_rasr) |= 1;
		}
		*(GPIO9_BASE + dr_clr) = 1u << MPUTEST_PIN0;
		cpuCycles = hal_timerGetCyc() - cpuCycles;


		hal_consolePrint(ATTR_BOLD, "--------------------------------------------------");
		lib_sprintf(b, "region disable/enable with %d regions - %d times: %d cycles (%d us)",
				allocCnt, ITER_CNT, cpuCycles, (int)hal_timerCyc2Us(cpuCycles));
		hal_consolePrint(ATTR_BOLD, b);


		for (int i = 0; i < ITER_CNT; i++) {
			__asm__ volatile("nop");
		}
	}


	// for (int i = 0; i < ITER_CNT; i++) {
	// 	__asm__ volatile("nop");
	// }

	// /* TEST cache invalidation */
	// *(GPIO9_BASE + dr_set) = 1u << MPUTEST_PIN0;
	// for (int i = 0; i < 100; i++) {
	// 	*(GPIO9_BASE + dr_set) = 1u << MPUTEST_PIN1;
	// 	hal_invalDCacheAll();
	// 	*(GPIO9_BASE + dr_set) = 1u << MPUTEST_PIN1;
	// }
	// *(GPIO9_BASE + dr_set) = 1u << MPUTEST_PIN0;

	for (int i = 0; i < ITER_CNT; i++) {
		__asm__ volatile("nop");
	}

#ifdef MPUTEST_ORGIMPL
	u32 t;
	for (int i = 0; i < syspage->hs.mpu.allocCnt; ++i) {
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
#endif
}

#ifdef MPUTEST_ORGIMPL

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

	hal_spinlockCreate(&pmap_common.lock, "pmap");


	pmap_switch_bulktest();
}

#else /* MPUTEST_ORGIMPL */

void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	unsigned int i;

	(*vstart) = (void *)(((ptr_t)_init_vectors + 7) & ~7);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */
	pmap->end = (void *)((addr_t)&__bss_start + 32 * 1024);

	/* Configure MPU */
	pmap_common.mpu = (void *)pmap_common.mpu;

	/* Disable MPU just in case */
	*(pmap_common.mpu + mpu_ctrl) &= ~1;
	hal_cpuDataMemoryBarrier();

	/* Allow unlimited kernel access */
	*(pmap_common.mpu + mpu_ctrl) |= (1 << 2);
	hal_cpuDataMemoryBarrier();

	for (i = 0; i < (u8)(syspage->hs.mpu.type >> 8); ++i) {
		/* Select region */
		*(pmap_common.mpu + mpu_rnr) = i;

		*(pmap_common.mpu + mpu_rasr) = 0;
		hal_cpuDataMemoryBarrier();
	}

	/* Enable MPU */
	*(pmap_common.mpu + mpu_ctrl) |= 1;
	hal_cpuDataMemoryBarrier();

	hal_spinlockCreate(&pmap_common.lock, "pmap");


	pmap_switch_bulktest();
}

#endif /* MPUTEST_ORGIMPL */
