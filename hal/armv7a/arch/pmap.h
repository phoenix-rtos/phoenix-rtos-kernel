/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem
 *
 * Copyright 2014, 2018 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_PMAP_ARMV7A_H_
#define _PH_HAL_PMAP_ARMV7A_H_

#include "hal/types.h"
#include "hal/page.h"

/* Predefined virtual addresses */
#define VADDR_KERNEL  0xc0000000 /* base virtual address of kernel space */
#define VADDR_MIN     0x00000000
#define VADDR_MAX     0xffffffff
#define VADDR_USR_MAX 0x80000000

/* (MOD) */
#define VADDR_SCRATCHPAD_TTL 0xfff00000


/* Architecture dependent page attributes */
#define PGHD_PRESENT    0x20
#define PGHD_NOT_CACHED 0x10
#define PGHD_USER       0x08
#define PGHD_WRITE      0x04
#define PGHD_EXEC       0x02
#define PGHD_DEV        0x01
#define PGHD_READ       0x00
#define PGHD_MASK       0x1f

#ifndef __ASSEMBLY__

typedef struct _pmap_t {
	u8 asid_ix;
	u32 *pdir;
	addr_t addr; /* physical address of pdir */
	void *start;
	void *end;
	void *pmapv;
	page_t *pmapp;
} pmap_t;

#endif

#endif
