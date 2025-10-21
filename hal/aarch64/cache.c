/*
 * Phoenix-RTOS
 *
 * Cache maintenance operations for AArch64
 *
 * Copyright 2024 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

/* parasoft-begin-suppress MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */

#include "aarch64.h"


static u64 hal_getICacheLineSize(void)
{
	u64 ctr = sysreg_read(ctr_el0);
	ctr = ctr & 0xfU;
	return (u64)4 << ctr;
}


void hal_cpuInvalInstrCache(ptr_t vstart, ptr_t vend)
{
	u64 ctr;
	hal_cpuDataSyncBarrier();
	ctr = hal_getICacheLineSize();
	while (vstart < vend) {
		__asm__ volatile("ic ivau, %0" : : "r"(vstart) : "memory");
		vstart += ctr;
	}

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


static u64 hal_getDCacheLineSize(void)
{
	u64 ctr = sysreg_read(ctr_el0);
	ctr = (ctr >> 16) & 0xfu;
	return (u64)4 << ctr;
}


void hal_cpuCleanDataCache(ptr_t vstart, ptr_t vend)
{
	u64 ctr;
	hal_cpuDataSyncBarrier();
	ctr = hal_getDCacheLineSize();
	while (vstart < vend) {
		__asm__ volatile("dc cvac, %0" : : "r"(vstart) : "memory");
		vstart += ctr;
	}

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void hal_cpuInvalDataCache(ptr_t vstart, ptr_t vend)
{
	u64 ctr;
	hal_cpuDataSyncBarrier();
	ctr = hal_getDCacheLineSize();
	while (vstart < vend) {
		__asm__ volatile("dc ivac, %0" : : "r"(vstart) : "memory");
		vstart += ctr;
	}

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void hal_cpuFlushDataCache(ptr_t vstart, ptr_t vend)
{
	u64 ctr;
	hal_cpuDataSyncBarrier();
	ctr = hal_getDCacheLineSize();
	while (vstart < vend) {
		__asm__ volatile("dc civac, %0" : : "r"(vstart) : "memory");
		vstart += ctr;
	}

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}

/* parasoft-end-suppress MISRAC2012-DIR_4_3 */
