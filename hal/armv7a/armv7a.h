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

#ifndef _HAL_ARMV7A_H_
#define _HAL_ARMV7A_H_

#include <arch/types.h>


/* Barriers */

static inline void hal_cpuDataMemoryBarrier(void)
{
	__asm__ volatile ("dmb");
}


static inline void hal_cpuDataSyncBarrier(void)
{
	__asm__ volatile ("dsb");
}


static inline void hal_cpuInstrBarrier(void)
{
	__asm__ volatile ("isb");
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


/* Invalidate TLB entries by ASID Match */
extern void hal_cpuInvalASID(u8 asid);


/* Invalidate Unified TLB by MVA */
extern void hal_cpuInvalVA(ptr_t vaddr);


extern void hal_cpuInvalVAASID(ptr_t vaddr);


/* Invalidate entire Unified TLB*/
extern void hal_cpuInvalTLB(void);


/* Read Translation Table Base Register 0 with properties */
extern addr_t hal_cpuGetTTBR0(void);


/* Set Translation Table Base Register 0 with properties */
extern void hal_cpuSetTTBR0(addr_t ttbr0);


/* Set ContextID = Process ID (pmap->pdir) and ASID */
extern void hal_cpuSetContextId(u32 id);


/* Get ContextID = Process ID (pmap->pdir) and ASID */
extern u32 hal_cpuGetContextId(void);


/* Core Management */

extern u32 hal_cpuGetMIDR(void);


extern u32 hal_cpuGetPFR0(void);


extern u32 hal_cpuGetPFR1(void);

#endif
