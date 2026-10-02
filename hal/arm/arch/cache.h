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


#ifndef _PH_ARM_ARCH_CACHE_H_
#define _PH_ARM_ARCH_CACHE_H_


#include "hal/types.h"

/* Invalidate entire branch predictor array */
void hal_cpuBranchInval(void);


/* Invalidate all instruction caches to PoU. Also flushes branch target cache */
void hal_cpuICacheInval(void);


/* Invalidate instruction cache by VA to PoU */
void hal_cpuInvalInstrCache(ptr_t vstart, ptr_t vend);


/* Clean Data or Unified cache line by MVA to PoC */
void hal_cpuCleanDataCache(ptr_t vstart, ptr_t vend);


/* Invalidate Data or Unified cache line by MVA to PoC */
void hal_cpuInvalDataCache(ptr_t vstart, ptr_t vend);


/* Clean and Invalidate Data or Unified cache line by MVA to PoC */
void hal_cpuFlushDataCache(ptr_t vstart, ptr_t vend);


#endif /* _PH_ARM_ARCH_CACHE_H_ */
