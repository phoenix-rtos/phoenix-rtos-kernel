/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem
 *
 * Copyright 2012, 2023 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Andrzej Stalke
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_IA32_PMAP_H_
#define _PH_HAL_IA32_PMAP_H_

/* Predefined virtual addresses */
#define VADDR_KERNEL  0xc0000000U /* base virtual address of kernel space */
#define VADDR_MIN     0x00000000U
#define VADDR_MAX     0xffffffffU
#define VADDR_USR_MAX VADDR_KERNEL

/* Attributes specifying different types of caching */
#define PGHD_PCD       0x10U
#define PGHD_PWT       0x08U
#define PGHD_CACHE_WB  0x0U
#define PGHD_CACHE_WT  PGHD_PWT
#define PGHD_CACHE_UCM PGHD_PCD
#define PGHD_CACHE_UC  (PGHD_PCD | PGHD_PWT)
/* Architecture dependent page attributes */
#define PGHD_PRESENT    0x01U
#define PGHD_USER       0x04U
#define PGHD_WRITE      0x02U
#define PGHD_EXEC       0x00U
#define PGHD_READ       0x00U
#define PGHD_DEV        PGHD_CACHE_UC
#define PGHD_NOT_CACHED PGHD_CACHE_UCM

#define PGHD_4MB        0x80U
#define PGHD_4MB_GLOBAL 0x100U
#define PGHD_4MB_PAT    0x1000U


/* Architecture dependent page table attributes */
#define PTHD_PRESENT 0x01U
#define PTHD_USER    0x04U
#define PTHD_WRITE   0x02U


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

#include "cpu.h"
#include "vm/types.h"


#define SIZE_PDIR SIZE_PAGE


/* Structure describing page - its should be aligned to 2^N boundary */
typedef struct _page_t {
	addr_t addr;
	struct _page_t *next;
	struct _page_t *prev;
	u8 idx;
	u8 flags;
} page_t;


typedef struct _pmap_t {
	u32 *pdir;
	addr_t cr3;
	void *start;
	void *end;
	void *pmapv;
	addr_t pmapp;
} pmap_t;


int _pmap_enter(u32 *pdir, addr_t *pt, addr_t paddr, void *vaddr, vm_attr_t attr, page_t *alloc, int tlbInval);

#endif

#endif
