/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem (ARMv7r)
 *
 * Copyright 2017, 2020, 2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_PMAP_ARMV7R_H_
#define _PH_HAL_PMAP_ARMV7R_H_

#include "hal/types.h"

#define PGHD_PRESENT    0x01U
#define PGHD_USER       0x04U
#define PGHD_WRITE      0x02U
#define PGHD_EXEC       0x00U
#define PGHD_DEV        0x00U
#define PGHD_NOT_CACHED 0x00U
#define PGHD_READ       0x00U

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
