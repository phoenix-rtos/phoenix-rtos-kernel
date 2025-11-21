/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * TLB handling
 *
 * Copyright 2023 Phoenix Systems
 * Author: Andrzej Stalke, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _PH_HAL_IA32_TLB_H_
#define _PH_HAL_IA32_TLB_H_


#include "pmap.h"
#include "hal/tlb/tlb.h"


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static inline void hal_tlbFlushLocal(const pmap_t *pmap)
{
	u32 tmpreg;
	(void)pmap;

	/* clang-format off */
	__asm__ volatile (
		"movl %%cr3, %0\n\t"
		"movl %0, %%cr3"
	: "=r" (tmpreg)
	:
	: "memory");
	/* clang-format on */

	return;
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static inline void hal_tlbInvalidateLocalEntry(const pmap_t *pmap, const void *vaddr)
{
	(void)pmap;

	/* clang-format off */
	__asm__ volatile (
		"invlpg (%0)"
	:
	: "r" (vaddr)
	: "memory" );
	/* clang-format on */

	return;
}


#endif
