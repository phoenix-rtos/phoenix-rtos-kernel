/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ARMv7 Cortex-R related routines
 *
 * Copyright 2021, 2024 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ARMV7R_H_
#define _HAL_ARMV7R_H_

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


/* Invalidate entire branch predictor array */
extern void hal_cpuBranchInval(void);


/* Invalidate all instruction caches to PoU. Also flushes branch target cache */
extern void hal_cpuICacheInval(void);


/* Clean Data or Unified cache line by MVA to PoC */
extern void hal_cpuCleanDataCache(ptr_t vstart, ptr_t vend);


/* Invalidate Data or Unified cache line by MVA to PoC */
extern void hal_cpuInvalDataCache(ptr_t vstart, ptr_t vend);


/* Clean and Invalidate Data or Unified cache line by MVA to PoC */
extern void hal_cpuFlushDataCache(ptr_t vstart, ptr_t vend);


/* Core Management */

extern u32 hal_cpuGetMIDR(void);


extern u32 hal_cpuGetPFR0(void);


extern u32 hal_cpuGetPFR1(void);

#endif
