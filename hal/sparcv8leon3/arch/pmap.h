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

#ifndef _HAL_LEON3_PMAP_H_
#define _HAL_LEON3_PMAP_H_


#define SIZE_PDIR 0x1000

/* Predefined virtual addresses */

#define VADDR_KERNEL  0xc0000000 /* base virtual address of kernel space */
#define VADDR_MIN     0x00000000
#define VADDR_MAX     0xffffffff
#define VADDR_USR_MAX 0x80000000

/* Architecture dependent page attributes */

#define PGHD_READ       (1 << 0)
#define PGHD_WRITE      (1 << 1)
#define PGHD_EXEC       (1 << 2)
#define PGHD_USER       (1 << 3)
#define PGHD_PRESENT    (1 << 4)
#define PGHD_DEV        (1 << 5)
#define PGHD_NOT_CACHED (1 << 6)

/* Page table entry types */

#define PAGE_INVALID 0x0u
#define PAGE_DESCR   0x1u
#define PAGE_ENTRY   0x2u

/* Page flags */

#define PAGE_FREE 0x00000001

#define PAGE_OWNER_BOOT   (0 << 1)
#define PAGE_OWNER_KERNEL (1 << 1)
#define PAGE_OWNER_APP    (2 << 1)

#define PAGE_KERNEL_SYSPAGE (1 << 4)
#define PAGE_KERNEL_CPU     (2 << 4)
#define PAGE_KERNEL_PTABLE  (3 << 4)
#define PAGE_KERNEL_PMAP    (4 << 4)
#define PAGE_KERNEL_STACK   (5 << 4)
#define PAGE_KERNEL_HEAP    (6 << 4)


/* Page access permissions */

#define PERM_USER_RO   0x0 /* User read-only */
#define PERM_USER_RW   0x1 /* User read-write */
#define PERM_USER_RX   0x2 /* User read-exec */
#define PERM_USER_RWX  0x3 /* User read-write-exec */
#define PERM_USER_XO   0x4 /* User exec-only */
#define PERM_SUPER_RW  0x5 /* Supervisor read-write */
#define PERM_SUPER_RX  0x6 /* Supervisor read-exec */
#define PERM_SUPER_RWX 0x7 /* Supervisor read-write-exec */


#ifndef __ASSEMBLY__


#include "types.h"


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
	page_t *pmapp;
} pmap_t;


#else


typedef struct _pmap_t {
	u32 mpr;
	void *start;
	void *end;
} pmap_t;


#endif /* NOMMU */


#endif /* __ASSEMBLY__ */


#endif
