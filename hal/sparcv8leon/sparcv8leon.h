/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * sparcv8leon related routines
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_SPARCV8LEON_H_
#define _PH_HAL_SPARCV8LEON_H_


#include <arch/cpu.h>
#include "hal/types.h"

#include "hal/sparcv8leon/srmmu.h"


#define LEON3_IOAREA 0xfff00000U


static inline void hal_cpuDataStoreBarrier(void)
{
	__asm__ volatile("stbar;");
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static inline u32 hal_cpuLoadAlternate(addr_t addr, const u32 asi)
{
	/* clang-format off */

	__asm__ volatile(
		"lda [%0] %c1, %0"
		: "+r"(addr)
		: "i"(asi)
	);

	/* clang-format on */

	return addr;
}


static inline void hal_cpuStoreAlternate(addr_t addr, const u32 asi, u32 val)
{
	/* clang-format off */

	__asm__ volatile(
		"sta %0, [%1] %c2"
		:
		: "r"(val), "r"(addr), "i"(asi)
	);

	/* clang-format on */
}


static inline void hal_cpuflushDCacheL1(void)
{
	hal_cpuStoreAlternate(0, ASI_FLUSH_DCACHE, 0);
}


static inline void hal_cpuflushICacheL1(void)
{
	u32 ccr = hal_cpuLoadAlternate(0, ASI_CACHE_CTRL);
	ccr |= CCR_FI;
	hal_cpuStoreAlternate(0, ASI_CACHE_CTRL, ccr);
}

/* Bypass MMU - store to physical address.
 * Use with care on GR712RC - errata 1.7.19.
 * Store may update data cache - flush it after use.
 */
static inline void hal_cpuStorePaddr(u32 *paddr, u32 val)
{
#ifndef NOMMU
	hal_cpuStoreAlternate((addr_t)paddr, ASI_MMU_BYPASS, val);
#else
	*paddr = val;
#endif
}

/* Bypass MMU - load from physical address */
static inline u32 hal_cpuLoadPaddr(u32 *paddr)
{
#ifndef NOMMU
	return hal_cpuLoadAlternate((addr_t)paddr, ASI_MMU_BYPASS);
#else
	return *paddr;
#endif
}


#endif
