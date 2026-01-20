/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem
 *
 * Copyright 2014, 2018, 2024 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_PMAP_AARCH64_H_
#define _PH_HAL_PMAP_AARCH64_H_

#include "hal/types.h"

/* Predefined virtual addresses */
#define VADDR_KERNEL  0xffffffffc0000000UL /* base virtual address of kernel space */
#define VADDR_MIN     0x0000000000000000UL
#define VADDR_MAX     0xffffffffffffffffUL
#define VADDR_USR_MAX 0x0000008000000000UL /* 2^39 bytes - maximum reachable with 3 translation levels at 4K granule */
#define VADDR_DTB     0xfffffffffff00000UL /* Last 1 MB of virtual space */


/* Architecture dependent page attributes */
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
	asid_t asid;
	u64 *ttl1;   /* Translation table at level 1 */
	addr_t addr; /* Physical address of ttl1 */
	void *start;
	void *end;
	void *pmapv;
	addr_t pmapp;
} pmap_t;


void _pmap_preinit(addr_t dtbStart, addr_t dtbEnd);


void *_pmap_halMapDevice(addr_t paddr, size_t pageOffs, size_t size);


#endif

#endif
