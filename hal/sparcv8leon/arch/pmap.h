/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_LEON3_PMAP_H_
#define _PH_HAL_LEON3_PMAP_H_


#include "vm/types.h"
#define SIZE_PDIR 0x1000U

/* Predefined virtual addresses */

#define VADDR_KERNEL  0xc0000000U /* base virtual address of kernel space */
#define VADDR_MIN     0x00000000U
#define VADDR_MAX     0xffffffffU
#define VADDR_USR_MAX 0x80000000U

/* Architecture dependent page attributes */

#define PGHD_READ       (1U << 0)
#define PGHD_WRITE      (1U << 1)
#define PGHD_EXEC       (1U << 2)
#define PGHD_USER       (1U << 3)
#define PGHD_PRESENT    (1U << 4)
#define PGHD_DEV        (1U << 5)
#define PGHD_NOT_CACHED (1U << 6)

/* Page table entry types */

#define PAGE_INVALID 0x0U
#define PAGE_DESCR   0x1U
#define PAGE_ENTRY   0x2U

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


/* Page access permissions */

#define PERM_USER_RO   0x0U /* User read-only */
#define PERM_USER_RW   0x1U /* User read-write */
#define PERM_USER_RX   0x2U /* User read-exec */
#define PERM_USER_RWX  0x3U /* User read-write-exec */
#define PERM_USER_XO   0x4U /* User exec-only */
#define PERM_SUPER_RW  0x5U /* Supervisor read-write */
#define PERM_SUPER_RX  0x6U /* Supervisor read-exec */
#define PERM_SUPER_RWX 0x7U /* Supervisor read-write-exec */


#ifndef __ASSEMBLY__


#include "hal/types.h"


#define PAGE_ALIGN(addr) (((addr_t)(addr)) & ~(SIZE_PAGE - 1U))
#define PAGE_OFFS(addr)  (((addr_t)(addr)) & (SIZE_PAGE - 1U))


typedef struct _page_t {
	addr_t addr;
	u8 idx;
	u16 flags;
	struct _page_t *next;
	struct _page_t *prev;
} page_t;


#ifndef NOMMU


typedef struct _pmap_t {
	u32 context;
	u32 *pdir1;
	addr_t addr; /* physical address of pdir */
	void *start;
	void *end;
	void *pmapv;
	addr_t pmapp;
} pmap_t;


#else


typedef struct _pmap_t {
	u32 mpr;
	void *start;
	void *end;
} pmap_t;


#endif /* NOMMU */


void *_pmap_halMap(addr_t paddr, void *va, size_t size, vm_attr_t attr);


void *pmap_halMap(addr_t paddr, void *va, size_t size, vm_attr_t attr);


void *_pmap_halMapDevice(addr_t paddr, size_t pageOffs, size_t size);


void _pmap_halInit(void);


#endif /* __ASSEMBLY__ */


#endif
