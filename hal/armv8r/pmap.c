/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem (ARMv8r)
 *
 * Copyright 2017, 2020-2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch/cpu.h>
#include <config.h>

#include "hal/pmap.h"
#include "hal/string.h"
#include "halsyspage.h"
#include "syspage.h"

/* Linker symbols */
extern unsigned int _end;
extern unsigned int __bss_start;

u8 _init_stack[NUM_CPUS][SIZE_INITIAL_KSTACK] __attribute__((aligned(8)));


static struct {
	volatile u32 *mpu;
	spinlock_t lock;
	int last_mpu_count;
} pmap_common;


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	// pmap->regions = pmap_common.kernelCodeRegion;

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


int pmap_addMap(pmap_t *pmap, unsigned int map)
{
	return 0;
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


void pmap_switch(pmap_t *pmap)
{
	unsigned int i;
	spinlock_ctx_t sc;

	if (pmap != NULL) {
		hal_spinlockSet(&pmap_common.lock, &sc);

		/* Disable MPU */
		// TODO: barriers
		//  hal_cpuDataMemoryBarrier();
		//  *(pmap_common.mpu + mpu_ctrl) &= ~1;
		//  asm volatile(
		//  	"mov r0, #0\n"
		//  	"MCR p15,4,r0,c6,c1,1\n"
		//  	::); // Disable all regions (EL2)
		// TODO: disable MPU not available in EL1
		asm volatile(
				"MRC p15, 0, %0, c1, c0, 0 ;"
				"BIC %0, %0, #1 ;"
				"MCR p15, 0, %0, c1, c0, 0 ;" ::"r"(0));

		for (i = 0; i < pmap->mpu.allocCnt; ++i) {
			/* Select region */
			asm volatile("MCR p15, 0, %0, c6, c2, 1" ::"r"(i));

			asm volatile("MCR p15, 0, %0, c6, c3, 0" ::"r"(pmap->mpu.table[i].rbar));
			asm volatile("MCR p15, 0, %0, c6, c3, 1" ::"r"(pmap->mpu.table[i].rlar));
		}

		/* Disable all remaining regions */
		for (; i < pmap_common.last_mpu_count; i++) {
			/* Select region */
			asm volatile("MCR p15, 0, %0, c6, c2, 1" ::"r"(i));

			asm volatile("MCR p15, 0, %0, c6, c3, 1" ::"r"(0));
		}
		pmap_common.last_mpu_count = pmap->mpu.allocCnt;


		/* Enable MPU */
		asm volatile(
				"MRC p15, 0, %0, c1, c0, 0 ;"
				"ORR %0, %0, #1 ;"
				"MCR p15, 0, %0, c1, c0, 0 ;" ::"r"(0));
		// *(pmap_common.mpu + mpu_ctrl) |= 1;
		// hal_cpuDataSyncBarrier();
		// TODO: barriers

		hal_spinlockClear(&pmap_common.lock, &sc);
	}
}


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


void pmap_switch_bulktest(int cacheopt)
{
}


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	// const syspage_map_t *ikmap;
	// unsigned int ikregion;
	unsigned int i, cnt = syspage->hs.mpu.allocCnt;

	(*vstart) = (void *)(((ptr_t)&_end + 7) & ~7u);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */
	pmap->end = (void *)((addr_t)&__bss_start + 32 * 1024);


	/* Enable all regions for kernel */
	// pmap->regions = (1 << cnt) - 1;

	pmap->mpu.allocCnt = 0; /* TODO: temporary to skip kernel mpu reconfiguration */

	/* Configure MPU */
	// pmap_common.mpu = MPU_BASE;

	hal_spinlockCreate(&pmap_common.lock, "pmap");
	if (cnt == 0) {
		return;
	}


	pmap_common.last_mpu_count = (syspage->hs.mpu.type >> 8) & 0xff;

	/* Disable MPU just in case */
	// *(pmap_common.mpu + mpu_ctrl) &= ~1;
	// hal_cpuDataMemoryBarrier();

	/* Activate background region for privileged code - if an address does not belong to any enabled region,
	 * the default memory map will be used to determine memory attributes. */
	// *(pmap_common.mpu + mpu_ctrl) |= (1 << 2);
	// hal_cpuDataMemoryBarrier();

	for (i = 0; i < cnt; ++i) {
		/* Select MPU region to configure */
		// *(pmap_common.mpu + mpu_rnr) = i;
		// hal_cpuDataMemoryBarrier();

		// *(pmap_common.mpu + mpu_rbar) = syspage->hs.mpu.table[i].rbar;
		// hal_cpuDataMemoryBarrier();

		/* Disable regions for now */
		// *(pmap_common.mpu + mpu_rlar) = syspage->hs.mpu.table[i].rlar & ~1u;
		// hal_cpuDataMemoryBarrier();
	}

	/* Enable MPU */
	// *(pmap_common.mpu + mpu_ctrl) |= 1;
	// hal_cpuDataMemoryBarrier();

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
	// ikmap = syspage_mapAddrResolve((addr_t)_pmap_init);
	// if (ikmap != NULL) {
	// 	ikregion = pmap_map2region(ikmap->id);
	// }

	// if ((ikmap == NULL) || (ikregion == 0)) {
	// 	hal_consolePrint(ATTR_BOLD, "pmap: Kernel code map not found or has no regions. Bad system config\n");
	// 	for (;;) {
	// 		hal_cpuHalt();
	// 	}
	// }

	// pmap_common.kernelCodeRegion = ikregion;


	/* Enable background region for EL1 */
	asm volatile(
			"MRC p15, 0, %0, c1, c0, 0 ;"
			"BIC %0, %0, #0x20000 ;"
			"MCR p15, 0, %0, c1, c0, 0 ;" ::"r"(0));
}
