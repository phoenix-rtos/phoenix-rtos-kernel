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

#ifndef _HAL_PMAP_H_
#define _HAL_PMAP_H_

/* Architecure dependent page attributes - used for mapping */
#define PGHD_PRESENT    0x01
#define PGHD_USER       0x04
#define PGHD_WRITE      0x02
#define PGHD_EXEC       0x00
#define PGHD_DEV        0x00
#define PGHD_NOT_CACHED 0x00

/* Page flags */
#define PAGE_FREE            0x00000001

#define PAGE_OWNER_BOOT      (0 << 1)
#define PAGE_OWNER_KERNEL    (1 << 1)
#define PAGE_OWNER_APP       (2 << 1)

#define PAGE_KERNEL_SYSPAGE  (1 << 4)
#define PAGE_KERNEL_CPU      (2 << 4)
#define PAGE_KERNEL_PTABLE   (3 << 4)
#define PAGE_KERNEL_PMAP     (4 << 4)
#define PAGE_KERNEL_STACK    (5 << 4)
#define PAGE_KERNEL_HEAP     (6 << 4)


#ifndef __ASSEMBLY__

#include "../cpu.h"

typedef struct _page_t {
	addr_t addr;
	u8 idx;
	u16 flags;
	struct _page_t *next;
} page_t;


typedef struct _pmap_t {
	u32 mpr;
	void *start;
	void *end;
} pmap_t;


typedef struct _mpur_t {
	u8 region;
	u32 base;
	u32 size;
	u8 subregions;
	int attr;
} mpur_t;


static inline int pmap_belongs(pmap_t *pmap, void *addr)
{
	return addr >= pmap->start && addr < pmap->end;
}


extern int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top);


extern int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr);


extern void pmap_switch(pmap_t *pmap);


extern int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc);


extern int pmap_remove(pmap_t *pmap, void *vaddr);


static inline addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	return (addr_t)vaddr;
}


extern void _pmap_init(pmap_t *pmap, void **start, void **end);


#endif

#endif
