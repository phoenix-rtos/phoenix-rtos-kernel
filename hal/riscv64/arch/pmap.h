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

#ifndef _PH_HAL_RISCV64_PMAP_H_
#define _PH_HAL_RISCV64_PMAP_H_

/* Predefined virtual addresses */
#define VADDR_KERNEL  0x0000003fc0000000UL /* base virtual address of kernel space */
#define VADDR_MIN     0x00000000U
#define VADDR_MAX     0xffffffffffffffffUL
#define VADDR_USR_MAX VADDR_KERNEL

#define VADDR_DTB 0xffffffffc0000000UL

/* Architecure dependent page attributes */
#define PGHD_PRESENT    0x01U
#define PGHD_READ       0x02U
#define PGHD_WRITE      0x04U
#define PGHD_EXEC       0x08U
#define PGHD_USER       0x10U
#define PGHD_DEV        0x00U
#define PGHD_NOT_CACHED 0x00U

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

/* satp register */
#define SATP_MODE_SV39 (8UL << 60)


#ifndef __ASSEMBLY__

#include "vm/types.h"
#include "hal/types.h"


#define SIZE_PDIR SIZE_PAGE

#define PAGE_ALIGN(addr) (((addr_t)(addr)) & ~(SIZE_PAGE - 1UL))
#define PAGE_OFFS(addr)  (((addr_t)(addr)) & (SIZE_PAGE - 1UL))


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


void *_pmap_halMap(addr_t paddr, void *va, size_t size, vm_attr_t attr);


void *pmap_halMap(addr_t paddr, void *va, size_t size, vm_attr_t attr);


void *_pmap_halMapDevice(addr_t paddr, size_t pageOffs, size_t size);


void _pmap_halInit(void);


#endif


#endif
