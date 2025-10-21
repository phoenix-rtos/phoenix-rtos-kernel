/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ARMv7 Cortex-A related routines
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_ARMV7A_H_
#define _PH_HAL_ARMV7A_H_

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


/* Invalidate TLB entries by ASID Match */
void hal_cpuInvalASID(u8 asid);


/* Invalidate Unified TLB by MVA */
void hal_cpuInvalVA(ptr_t vaddr);


/* Invalidate entire Unified TLB*/
void hal_cpuInvalTLB(void);


/* Invalidate TLB entries by ASID Match on all cores in Inner Shareable domain */
void hal_cpuInvalASID_IS(u8 asid);


/* Invalidate Unified TLB by MVA on all cores in Inner Shareable domain */
void hal_cpuInvalVA_IS(ptr_t vaddr);


/* Invalidate entire Unified TLB on all cores in Inner Shareable domain */
void hal_cpuInvalTLB_IS(void);


/* Read Translation Table Base Register 0 with properties */
addr_t hal_cpuGetTTBR0(void);


/* Set Translation Table Base Register 0 with properties */
void hal_cpuSetTTBR0(addr_t ttbr0);


/* Set ContextID = Process ID (pmap->pdir) and ASID */
void hal_cpuSetContextId(u32 id);


/* Get ContextID = Process ID (pmap->pdir) and ASID */
u32 hal_cpuGetContextId(void);


/* Core Management */

u32 hal_cpuGetMIDR(void);


u32 hal_cpuGetPFR0(void);


u32 hal_cpuGetPFR1(void);


/* parasoft-end-suppress MISRAC2012-RULE_8_6*/

#endif
