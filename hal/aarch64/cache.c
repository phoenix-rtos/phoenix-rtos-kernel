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


void hal_cpuICacheInval(void)
{
	__asm__ volatile("dsb ish\n ic iallu\n dsb ish\n isb\n");
}


static u64 hal_getCacheLineSize(void)
{
	u64 ctr = sysreg_read(ctr_el0);
	ctr = (ctr >> 16) & 0xf;
	return 4 << ctr;
}

void hal_cpuCleanDataCache(ptr_t vstart, ptr_t vend)
{
	u64 ctr;
	hal_cpuDataSyncBarrier();
	ctr = hal_getCacheLineSize();
	while (vstart < vend) {
		asm volatile("dc cvac, %0" : : "r"(vstart) : "memory");
		vstart += ctr;
	}

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void hal_cpuInvalDataCache(ptr_t vstart, ptr_t vend)
{
	u64 ctr;
	hal_cpuDataSyncBarrier();
	ctr = hal_getCacheLineSize();
	while (vstart < vend) {
		asm volatile("dc ivac, %0" : : "r"(vstart) : "memory");
		vstart += ctr;
	}

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void hal_cpuFlushDataCache(ptr_t vstart, ptr_t vend)
{
	u64 ctr;
	hal_cpuDataSyncBarrier();
	ctr = hal_getCacheLineSize();
	while (vstart < vend) {
		asm volatile("dc civac, %0" : : "r"(vstart) : "memory");
		vstart += ctr;
	}

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}
