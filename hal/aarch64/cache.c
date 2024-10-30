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

#include "aarch64.h"


static u64 hal_getICacheLineSize(void)
{
	u64 ctr = sysreg_read(ctr_el0);
	ctr = ctr & 0xf;
	return 4 << ctr;
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
	ctr = (ctr >> 16) & 0xf;
	return 4 << ctr;
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
