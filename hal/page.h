/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * shared page definitions used in pmap
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jan Wiśniewski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_PAGE_H_
#define _HAL_PAGE_H_

#include "hal/types.h"

/* Page flags */
#define PAGE_FREE (0x00000001U)

#define PAGE_OWNER_BOOT   (0U << 1)
#define PAGE_OWNER_KERNEL (1U << 1)
#define PAGE_OWNER_APP    (2U << 1)

#define PAGE_KERNEL_SYSPAGE (1U << 4)
#define PAGE_KERNEL_CPU     (2U << 4)
#define PAGE_KERNEL_PTABLE  (3U << 4)
#define PAGE_KERNEL_PMAP    (4U << 4)
#define PAGE_KERNEL_STACK   (5U << 4)
#define PAGE_KERNEL_HEAP    (6U << 4)

#define PAGE_ALIGN(addr) (((addr_t)addr) & ~(SIZE_PAGE - 1))
#define PAGE_OFFS(addr)  (((addr_t)addr) & (SIZE_PAGE - 1))

#ifndef __ASSEMBLY__

/* Structure describing page - its should be aligned to 2^N boundary */
typedef struct _page_t {
	addr_t addr;
	u8 idx;
	u8 flags;
	struct _page_t *next;
#ifndef NOMMU
	struct _page_t *prev;
#endif
} page_t;

#endif

#endif
