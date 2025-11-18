/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem
 *
 * Copyright 2014, 2018, 2024 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_PMAP_AARCH64_H_
#define _PH_HAL_PMAP_AARCH64_H_

#include "hal/types.h"
#include "hal/page.h"

/* Predefined virtual addresses */
#define VADDR_KERNEL  0xffffffffc0000000 /* base virtual address of kernel space */
#define VADDR_MIN     0x0000000000000000
#define VADDR_MAX     0xffffffffffffffff
#define VADDR_USR_MAX 0x0000008000000000 /* 2^39 bytes - maximum reachable with 3 translation levels at 4K granule */
#define VADDR_DTB     0xfffffffffff00000 /* Last 1 MB of virtual space */


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
	asid_t asid;
	u64 *ttl1;   /* Translation table at level 1 */
	addr_t addr; /* Physical address of ttl1 */
	void *start;
	void *end;
	void *pmapv;
	page_t *pmapp;
} pmap_t;


void _pmap_preinit(addr_t dtbStart, addr_t dtbEnd);


void *_pmap_halMapDevice(addr_t paddr, size_t pageOffs, size_t size);


#endif

#endif
