/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem (ARMv8)
 *
 * Copyright 2017, 2020, 2022 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_PMAP_ARMV8M_H_
#define _HAL_PMAP_ARMV8M_H_

#include "hal/types.h"
#include "syspage.h"

#define PGHD_PRESENT    0x01u
#define PGHD_USER       0x04u
#define PGHD_WRITE      0x02u
#define PGHD_EXEC       0x00u
#define PGHD_DEV        0x00u
#define PGHD_NOT_CACHED 0x00u
#define PGHD_READ       0x00u

/* Page flags */
#define PAGE_FREE 0x00000001u

#define PAGE_OWNER_BOOT   (0u << 1)
#define PAGE_OWNER_KERNEL (1u << 1)
#define PAGE_OWNER_APP    (2u << 1)

#define PAGE_KERNEL_SYSPAGE (1u << 4)
#define PAGE_KERNEL_CPU     (2u << 4)
#define PAGE_KERNEL_PTABLE  (3u << 4)
#define PAGE_KERNEL_PMAP    (4u << 4)
#define PAGE_KERNEL_STACK   (5u << 4)
#define PAGE_KERNEL_HEAP    (6u << 4)


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
	hal_syspage_prog_t hal;
} pmap_t;

#endif

#endif
