/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem
 *
 * Copyright 2014, 2018 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_PMAP_ARMV7A_H_
#define _PH_HAL_PMAP_ARMV7A_H_

#include "hal/types.h"

/* Predefined virtual adresses */
#define VADDR_KERNEL  0xc0000000U /* base virtual address of kernel space */
#define VADDR_MIN     0x00000000U
#define VADDR_MAX     0xffffffffU
#define VADDR_USR_MAX 0x80000000U

/* (MOD) */
#define VADDR_SCRATCHPAD_TTL 0xfff00000


/* Architecure dependent page attributes */
#define PGHD_PRESENT    0x20U
#define PGHD_NOT_CACHED 0x10U
#define PGHD_USER       0x08U
#define PGHD_WRITE      0x04U
#define PGHD_EXEC       0x02U
#define PGHD_DEV        0x01U
#define PGHD_READ       0x00U
#define PGHD_MASK       0x1fU


/* Page flags */
#define PAGE_FREE 0x00000001U

#define PAGE_OWNER_BOOT   (0U << 1)
#define PAGE_OWNER_KERNEL (1U << 1)
#define PAGE_OWNER_APP    (2U << 1)

#define PAGE_KERNEL_SYSPAGE (1U << 4)
#define PAGE_KERNEL_CPU     (2U << 4)
#define PAGE_KERNEL_PTABLE  (3U << 4)
#define PAGE_KERNEL_PMAP    (4U << 4)
#define PAGE_KERNEL_STACK   (5U << 4)
#define PAGE_KERNEL_HEAP    (6U << 4)

#ifndef __ASSEMBLY__

/* Structure describing page - its should be aligned to 2^N boundary */
typedef struct _page_t {
	addr_t addr;
	u8 idx;
	u8 flags;
	struct _page_t *next;
	struct _page_t *prev;
} page_t;


typedef struct _pmap_t {
	u8 asid_ix;
	u32 *pdir;
	addr_t addr; /* physical address of pdir */
	void *start;
	void *end;
	void *pmapv;
	page_t *pmapp;
} pmap_t;

#endif

#endif
