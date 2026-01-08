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

#include "hal/types.h"
#include "hal/page.h"

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

#define PAGE_INVALID 0x0U
#define PAGE_DESCR   0x1U
#define PAGE_ENTRY   0x2U


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
	void *start;
	void *end;
} pmap_t;


#endif /* NOMMU */


void *_pmap_halMap(addr_t paddr, void *va, size_t size, int attr);


void *pmap_halMap(addr_t paddr, void *va, size_t size, int attr);


void *_pmap_halMapDevice(addr_t paddr, size_t pageOffs, size_t size);


void _pmap_halInit(void);


#endif /* __ASSEMBLY__ */


#endif
