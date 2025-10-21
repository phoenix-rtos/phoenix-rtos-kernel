/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ARMv8 Cortex-R related routines
 *
 * Copyright 2021, 2024 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_ARMV8R_H_
#define _PH_HAL_ARMV8R_H_

#include "hal/types.h"


/* Barriers */


static inline void hal_cpuDataMemoryBarrier(void)
{
	__asm__ volatile("dmb");
}


static inline void hal_cpuDataSyncBarrier(void)
{
	__asm__ volatile("dsb");
}


static inline void hal_cpuInstrBarrier(void)
{
	__asm__ volatile("isb");
}


/* Memory Management */

/* parasoft-begin-suppress MISRAC2012-RULE_8_6 "Each function has definition in assembly code" */

/* Invalidate entire branch predictor array */
void hal_cpuBranchInval(void);


/* Invalidate all instruction caches to PoU. Also flushes branch target cache */
void hal_cpuICacheInval(void);


/* Clean Data or Unified cache line by MVA to PoC */
void hal_cpuCleanDataCache(ptr_t vstart, ptr_t vend);


/* Invalidate Data or Unified cache line by MVA to PoC */
void hal_cpuInvalDataCache(ptr_t vstart, ptr_t vend);


/* Clean and Invalidate Data or Unified cache line by MVA to PoC */
void hal_cpuFlushDataCache(ptr_t vstart, ptr_t vend);


/* Core Management */

u32 hal_cpuGetMIDR(void);


u32 hal_cpuGetPFR0(void);


u32 hal_cpuGetPFR1(void);

/* parasoft-end-suppress MISRAC2012-RULE_8_6*/

#endif
