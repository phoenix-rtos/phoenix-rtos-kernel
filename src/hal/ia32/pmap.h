/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem
 *
 * Copyright 2012 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_PMAP_H_
#define _HAL_PMAP_H_

#define SIZE_PAGE      0x1000
#define SIZE_PDIR      SIZE_PAGE


/* Predefined virtual adresses */
#define VADDR_KERNEL   0xc0000000   /* base virtual address of kernel space */
#define VADDR_MIN      0x00000000
#define VADDR_MAX      0xffffffff
#define VADDR_USR_MAX  VADDR_KERNEL


/* Architecure dependent page attributes */
#define PGHD_PRESENT    0x01
#define PGHD_USER       0x04
#define PGHD_WRITE      0x02
#define PGHD_EXEC       0x00
#define PGHD_DEV        0x00
#define PGHD_NOT_CACHED 0x00


/* Architecure dependent page table attributes */
#define PTHD_PRESENT  0x01
#define PTHD_USER     0x04
#define PTHD_WRITE    0x02


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


/* Structure describing page - its should be aligned to 2^N boundary */
typedef struct _page_t {
	addr_t addr;
	u8 idx;
	u8 flags;
	struct _page_t *next;
	struct _page_t *prev;
} page_t;


typedef struct _pmap_t {
	u32 *pdir;
	addr_t cr3;
	void *start;
	void *end;
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


extern void _pmap_init(pmap_t *pmap, void **start, void **end);

#endif

#endif
