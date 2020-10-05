/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem (ARMv7 with MPU)
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_PMAP_H_
#define _HAL_PMAP_H_

/* TODO */
/* Predefined virutal adresses */
#define VADDR_MIN       0x20000000
#define VADDR_KERNEL    0x20000000
#define VADDR_KERNELSZ  (320 * 1024)
#define VADDR_MAX       (VADDR_KERNEL + RAM_SIZE * 1024)

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

#include "cpu.h"


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


static inline addr_t pmap_getMinVAdrr(void)
{
	return VADDR_MIN;
}


static inline addr_t pmap_getMaxVAdrr(void)
{
	return VADDR_MAX;
}


extern int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr);


extern void pmap_switch(pmap_t *pmap);


extern int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc);


extern int pmap_remove(pmap_t *pmap, void *vaddr);


static inline addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	return (addr_t)vaddr;
}


static inline int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top)
{
	if (i)
		return -1;

	*vaddr = (void *)VADDR_KERNEL;
	*size = (((size_t)(*top) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1)) - (size_t)VADDR_KERNEL;

	return 0;
}


extern int pmap_getMapsCnt(void);


extern int pmap_getMapParameters(u8 id, void **start, void **end);


extern void pmap_getAllocatedSegment(void *memStart, void *memStop, void **segStart, void **segStop);


extern void _pmap_init(pmap_t *pmap, void **start, void **end);


#endif

#endif
