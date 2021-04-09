/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem (RISCV64)
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_PMAP_H_
#define _HAL_PMAP_H_


/* Predefined virtual addresses */
#define VADDR_KERNEL   0x0000003fc0000000L   /* base virtual address of kernel space */
#define VADDR_MIN      0x00000000
#define VADDR_MAX      0xffffffffffffffffL
#define VADDR_USR_MAX  VADDR_KERNEL


/* Architecure dependent page attributes */
#define PGHD_PRESENT    0x01
#define PGHD_READ       0x02
#define PGHD_WRITE      0x04
#define PGHD_EXEC       0x08
#define PGHD_USER       0x10
#define PGHD_DEV        0x00
#define PGHD_NOT_CACHED 0x00


/* Architecure dependent page table attributes */
#define PTHD_PRESENT  0x01
#define PTHD_READ     0x02
#define PTHD_WRITE    0x04
#define PTHD_EXEC     0x08
#define PTHD_USER     0x10


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


#define SIZE_PDIR SIZE_PAGE


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


static inline int pmap_belongs(pmap_t *pmap, void *addr)
{
	return addr >= pmap->start && addr < pmap->end;
}


extern int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr);


extern addr_t pmap_destroy(pmap_t *pmap, int *i);


extern void pmap_switch(pmap_t *pmap);


extern int pmap_enter(pmap_t *pmap, addr_t addr, void *vaddr, int attrs, page_t *alloc);


/* Function removes mapping for given address */
extern int pmap_remove(pmap_t *pmap, void *vaddr);


extern addr_t pmap_resolve(pmap_t *pmap, void *vaddr);


extern int pmap_getPage(page_t *page, addr_t *addr);


/* Function return character marker for page flags */
extern char pmap_marker(page_t *p);


/* Function allocates page tables for kernel space */
extern int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp);


extern int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top);


extern int pmap_getMapsCnt(void);


extern int pmap_getMapParameters(u8 id, void **start, void **end);


extern void pmap_getAllocatedSegment(void *memStart, void *memStop, void **segStart, void **segStop);


extern void _pmap_init(pmap_t *pmap, void **start, void **end);

#endif

#endif
