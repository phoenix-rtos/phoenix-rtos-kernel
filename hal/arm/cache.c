/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ARM Cortex-A/R cache operations
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <arch/cache.h>
#include <arch/cpu.h>
#include "barriers.h"


/* parasoft-begin-suppress MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */


static inline void hal_arm_writeBPIALL(void)
{
	__asm__ volatile("mcr p15, 0, %0, c7, c5, 6" ::"r"(0));
}


static inline void hal_arm_writeICIALLU(void)
{
	__asm__ volatile("mcr p15, 0, %0, c7, c5, 0" ::"r"(0) : "memory");
}


static inline void hal_arm_writeICIMVAU(u32 mva)
{
	__asm__ volatile("mcr p15, 0, %0, c7, c5, 1" ::"r"(mva) : "memory");
}


static inline void hal_arm_writeDCIMVAC(u32 mva)
{
	__asm__ volatile("mcr p15, 0, %0, c7, c6, 1" ::"r"(mva) : "memory");
}


static inline void hal_arm_writeDCCMVAC(u32 mva)
{
	__asm__ volatile("mcr p15, 0, %0, c7, c10, 1" ::"r"(mva) : "memory");
}


static inline void hal_arm_writeDCCIMVAC(u32 mva)
{
	__asm__ volatile("mcr p15, 0, %0, c7, c14, 1" ::"r"(mva) : "memory");
}


static inline u32 hal_arm_readCTR(void)
{
	u32 ret;
	__asm__ volatile("mrc p15, 0, %0, c0, c0, 1" : "=r"(ret));
	return ret;
}


/* parasoft-end-suppress MISRAC2012-DIR_4_3 */


static void hal_cacheAddressLoop(u32 start, u32 end, u32 logLineSize, void (*regWrite)(u32 mva))
{
	u32 lineSize, lineMask;
	if (start == end) {
		return;
	}

	hal_cpuDataSyncBarrier();
	lineSize = 4UL << logLineSize;
	lineMask = ~(lineSize - 1U);
	start = (u32)start & lineMask;
	/* Special case - handle last line of address space to ensure there is never overflow in the loop below. */
	if ((end > lineMask) || (end == 0U)) {
		end = lineMask;
		regWrite(end);
	}

	while (start < end) {
		regWrite(start);
		start += lineSize;
	}

	hal_cpuInstrBarrier();
	hal_cpuDataSyncBarrier();
}


void hal_cpuBranchInval(void)
{
	hal_cpuDataSyncBarrier();
	hal_arm_writeBPIALL();
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void hal_cpuICacheInval(void)
{
	hal_cpuDataSyncBarrier();
	hal_arm_writeICIALLU();
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void hal_cpuInvalInstrCache(ptr_t vstart, ptr_t vend)
{
	u32 logLineSize = hal_arm_readCTR() & 0xfU;
	hal_cacheAddressLoop(vstart, vend, logLineSize, hal_arm_writeICIMVAU);
}


void hal_cpuCleanDataCache(ptr_t vstart, ptr_t vend)
{
	u32 logLineSize = (hal_arm_readCTR() >> 16) & 0xfU;
	hal_cacheAddressLoop(vstart, vend, logLineSize, hal_arm_writeDCCMVAC);
}


void hal_cpuInvalDataCache(ptr_t vstart, ptr_t vend)
{
	u32 logLineSize = (hal_arm_readCTR() >> 16) & 0xfU;
	hal_cacheAddressLoop(vstart, vend, logLineSize, hal_arm_writeDCIMVAC);
}


void hal_cpuFlushDataCache(ptr_t vstart, ptr_t vend)
{
	u32 logLineSize = (hal_arm_readCTR() >> 16) & 0xfU;
	hal_cacheAddressLoop(vstart, vend, logLineSize, hal_arm_writeDCCIMVAC);
}
