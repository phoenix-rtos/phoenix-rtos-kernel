/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem (ARMv7 with MPU)
 *
 * Copyright 2017, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_PMAP_ARMV7M_H_
#define _PH_HAL_PMAP_ARMV7M_H_

#include "hal/types.h"

/* Architecure dependent page attributes - used for mapping */
#define PGHD_PRESENT    0x01
#define PGHD_USER       0x04
#define PGHD_WRITE      0x02
#define PGHD_EXEC       0x00
#define PGHD_DEV        0x00
#define PGHD_NOT_CACHED 0x00
#define PGHD_READ       0x00

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


#ifndef __ASSEMBLY__

typedef struct _page_t {
	addr_t addr;
	u8 idx;
	u16 flags;
	struct _page_t *next;
} page_t;


typedef struct _pmap_t {
	void *start;
	void *end;
	u32 regions;
} pmap_t;

#endif

#endif
