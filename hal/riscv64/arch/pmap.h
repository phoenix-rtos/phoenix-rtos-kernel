/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem (RISCV64)
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_RISCV64_PMAP_H_
#define _HAL_RISCV64_PMAP_H_

/* Predefined virtual addresses */
#define VADDR_MIN     0x00000000
#define VADDR_MAX     0xffffffffffffffffL
#define VADDR_USR_MAX VADDR_KERNEL

#define VADDR_DTB 0xffffffffc0000000UL

/* Architecure dependent page attributes */
#define PGHD_PRESENT    0x01
#define PGHD_READ       0x02
#define PGHD_WRITE      0x04
#define PGHD_EXEC       0x08
#define PGHD_USER       0x10
#define PGHD_DEV        0x00
#define PGHD_NOT_CACHED 0x00

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

/* satp register */
#define SATP_MODE_SV39 (8UL << 60)


#ifndef __ASSEMBLY__

#include "hal/types.h"


#define SIZE_PDIR SIZE_PAGE

#define PAGE_ALIGN(addr) (((addr_t)addr) & ~(SIZE_PAGE - 1))
#define PAGE_OFFS(addr)  (((addr_t)addr) & (SIZE_PAGE - 1))


/* Structure describing page - its should be aligned to 2^N boundary */
typedef struct _page_t {
	addr_t addr;
	u8 idx;
	u8 flags;
	struct _page_t *next;
	struct _page_t *prev;
} page_t;


typedef struct _pmap_t {
	u64 *pdir2;
	addr_t satp;
	void *start;
	void *end;
	void *pmapv;
	page_t *pmapp;
} pmap_t;


/* Get kernel physical address */
addr_t pmap_getKernelStart(void);


void *_pmap_halMap(addr_t paddr, void *va, size_t size, int attr);


void *pmap_halMap(addr_t paddr, void *va, size_t size, int attr);


void *_pmap_halMapDevice(addr_t paddr, size_t pageOffs, size_t size);


void _pmap_halInit(void);


#endif


#endif
