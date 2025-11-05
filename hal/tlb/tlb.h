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

#ifndef _PH_HAL_TLB_H_
#define _PH_HAL_TLB_H_


#include <arch/spinlock.h>


/* To invalidate entire TLB set vaddr = NULL & count = 0.
 * Must be protected by pmap_common.lock */
void hal_tlbInvalidateEntry(const pmap_t *pmap, const void *vaddr, size_t count);


/* Must be protected by pmap_common.lock */
void hal_tlbCommit(spinlock_t *spinlock, spinlock_ctx_t *ctx);


void hal_tlbShootdown(void);


void hal_tlbInitCore(const unsigned int id);


#endif
