/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * TLB handling
 *
 * Copyright 2023 Phoenix Systems
 * Author; Andrzej Stalke
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_TLB_H_
#define _HAL_TLB_H_

#include <arch/types.h>
#include "../spinlock.h"

static inline void hal_tlbFlushLocal(void)
{
	u32 tmpreg;
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


static inline void hal_tlbInvalidateLocalEntry(void const *vaddr)
{
	/* clang-format off */
	__asm__ volatile (
		"invlpg (%0)"
	:
	: "r" (vaddr)
	: "memory" );
	/* clang-format on */

	return;
}

void hal_tlbFlush(void);


void hal_tlbInvalidateEntry(void const *vaddr);


void hal_tlbCommit(spinlock_t *spinlock, spinlock_ctx_t *ctx);


void hal_tlbShootdown(void);


void hal_tlbInitCore(const unsigned int id);

#endif
