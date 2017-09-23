/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "pmap.h"
#include "syspage.h"
#include "spinlock.h"
#include "string.h"
#include "console.h"
#include "lib/lib.h"

#include "../../../include/errno.h"
#include "../../../include/mman.h"


extern void _end(void);
extern void _etext(void);


__attribute__((aligned(SIZE_PAGE)))
struct {
	u8 heap[SIZE_PAGE];
	u32 minAddr;
	u32 maxAddr;
	addr_t *ptable;
	u32 start;
	u32 end;
	spinlock_t lock;
} pmap_common;


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	int i, pages;
	pmap->pdir = vaddr;
	pmap->cr3 = p->addr;

	/* Copy kernel page tables */
	hal_memset(pmap->pdir, 0, 4096);
	vaddr = (void *)((VADDR_KERNEL + SIZE_PAGE) & ~(SIZE_PAGE - 1));

	pages = (kpmap->end - vaddr) / (SIZE_PAGE << 10);
	for (i = 0; i < pages; vaddr += (SIZE_PAGE << 10), ++i)
		pmap->pdir[(u32) vaddr >> 22] = kpmap->pdir[(u32) vaddr >> 22];

	return EOK;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	int kernel = ((VADDR_KERNEL + SIZE_PAGE) & ~(SIZE_PAGE - 1)) >> 22;

	while (*i < kernel) {
		if (pmap->pdir[*i] != NULL)
			return pmap->pdir[(*i)++] & ~0xfff;
		(*i)++;
	}

	return 0;
}


void pmap_switch(pmap_t *pmap)
{
	hal_cpuSwitchSpace(pmap->cr3);
}


/* Functions maps page at specified address */
int pmap_enter(pmap_t *pmap, addr_t pa, void *va, int attr, page_t *alloc)
{
	unsigned int pdi, pti;
	addr_t addr, *ptable;

	pdi = (u32)va >> 22;
	pti = ((u32)va >> 12) & 0x000003ff;

	/* If no page table is allocated add new one */
	if (!pmap->pdir[pdi]) {
		if (alloc == NULL)
			return -EFAULT;
		pmap->pdir[pdi] = ((alloc->addr & ~0xfff) | (attr & 0xfff) | PTHD_USER | PTHD_WRITE | PTHD_PRESENT);
	}

	hal_spinlockSet(&pmap_common.lock);

	/* Map selected page table to specified virtual address */
	addr = pmap->pdir[pdi];
	if ((u32)pmap_common.ptable < VADDR_KERNEL) {
		hal_spinlockClear(&pmap_common.lock);
		return -EFAULT;
	}

	/* Map selected page table */
	ptable = (addr_t *)(syspage->ptable + VADDR_KERNEL);
	ptable[((u32)pmap_common.ptable >> 12) & 0x000003ff] = (addr & ~0xfff) | (PGHD_WRITE | PGHD_PRESENT | PGHD_USER);

	hal_cpuFlushTLB(pmap_common.ptable);

	/* And at last map page or only changle attributes of map entry */
	pmap_common.ptable[pti] = ((pa & ~0xfff) | (attr & 0xfff) | PGHD_PRESENT);

	hal_cpuFlushTLB(va);

	hal_spinlockClear(&pmap_common.lock);

	return EOK;
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	unsigned int pdi, pti;
	addr_t addr, *ptable;

	pdi = (u32)vaddr >> 22;
	pti = ((u32)vaddr >> 12) & 0x000003ff;

	/* no page table is allocated => page is not mapped */
	if (!pmap->pdir[pdi])
		return EOK;

	hal_spinlockSet(&pmap_common.lock);

	/* Map selected page table to specified virtual address */
	addr = pmap->pdir[pdi];
	if ((u32)pmap_common.ptable < VADDR_KERNEL) {
		hal_spinlockClear(&pmap_common.lock);
		return -EFAULT;
	}

	/* Map selected page table */
	ptable = (addr_t *)(syspage->ptable + VADDR_KERNEL);
	ptable[((u32)pmap_common.ptable >> 12) & 0x000003ff] = (addr & ~0xfff) | (PGHD_WRITE | PGHD_PRESENT);

	hal_cpuFlushTLB(pmap_common.ptable);

	/* Unmap page */
	pmap_common.ptable[pti] = 0;

	hal_cpuFlushTLB(vaddr);
	hal_spinlockClear(&pmap_common.lock);

	return EOK;
}


/* Functions returs physical address associated with specified virtual address */
addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	unsigned int pdi, pti;
	addr_t addr, *ptable;

	pdi = (u32)vaddr >> 22;
	pti = ((u32)vaddr >> 12) & 0x000003ff;

	if (!pmap->pdir[pdi])
		return 0;

	hal_spinlockSet(&pmap_common.lock);

	/* Map page table corresponding to vaddr at specified virtual address */
	addr = pmap->pdir[pdi];

	ptable = (addr_t *)(syspage->ptable + VADDR_KERNEL);
	ptable[((u32)pmap_common.ptable >> 12) & 0x000003ff] = (addr & ~0xfff) | (PGHD_WRITE | PGHD_PRESENT);
	hal_cpuFlushTLB(pmap_common.ptable);

	addr = (addr_t)pmap_common.ptable[pti];
	hal_spinlockClear(&pmap_common.lock);

	/*if (((*paddr) & PGHD_PRESENT) == 0)
		return -ENOMEM;

	*paddr = ((*paddr) & ~0xfff) | ((u32)vaddr & 0xfff);*/

	return addr;
}


/* Function fills page_t structure for frame given by addr */
int pmap_getPage(page_t *page, addr_t *addr)
{
	unsigned int k;
	addr_t a, ta;
	u16 tl;
	syspage_program_t *p;

	a = *addr & ~0xfff;
	page->flags = 0;

	/* Test address ranges */
	hal_spinlockSet(&pmap_common.lock);

	if (a < pmap_common.minAddr)
		a = pmap_common.minAddr;

	if (a >= pmap_common.maxAddr) {
		hal_spinlockClear(&pmap_common.lock);
		return -ENOMEM;
	}

	hal_spinlockClear(&pmap_common.lock);

	page->addr = a;
	page->flags = 0;

	for (k = 0; k < syspage->mmsize; k++) {

		if ((a >= syspage->mm[k].addr) && (a <= syspage->mm[k].addr + syspage->mm[k].len - 1) &&
			(a + SIZE_PAGE - 1 >= syspage->mm[k].addr) && (a + SIZE_PAGE - 1 <= syspage->mm[k].addr + syspage->mm[k].len - 1)) {
			if (syspage->mm[k].attr == 1)
				page->flags |= PAGE_FREE;
			break;
		}

		/* Test if next memory area is higher than current page */
		if ((syspage->mm[k].addr > a) && (syspage->mm[k].addr > a + SIZE_PAGE - 1)) {
			a = (syspage->mm[k].addr & ~0xfff) - SIZE_PAGE;
			k = syspage->mmsize;
			break;
		}

		/* Test if page overlaps with reserved memory area */
		if ((a <= syspage->mm[k].addr) && (syspage->mm[k].attr != 1)) {
			page->flags &= ~PAGE_FREE;
			break;
		}
	}
	*addr = a + SIZE_PAGE;

	/* If page doesn't exist return error */
	if (k == syspage->mmsize)
		return -EINVAL;

	/* Page is allocated by BIOS */
	if (!(page->flags & PAGE_FREE)) {
		page->flags |= PAGE_OWNER_BOOT;
		return EOK;
	}

	/* Verify if page has been allocated by loader or kernel */
	hal_spinlockSet(&pmap_common.lock);

	/*if ((page->addr >= (u32)pmap_common.ptable - VADDR_KERNEL) && (page->addr < (u32)pmap_common.ptable - VADDR_KERNEL + SIZE_PAGE)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_PMAP);
	}*/

	hal_spinlockClear(&pmap_common.lock);

	if ((page->addr >= (u32)syspage - VADDR_KERNEL) && (page->addr < (u32)syspage - VADDR_KERNEL + SIZE_PAGE)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_SYSPAGE);
	}

	/* Check addr according GDT parameters */
	ta = syspage->gdtr.addr;
	tl = syspage->gdtr.limit;
	if ((page->addr >= ta - VADDR_KERNEL) && (page->addr < ta - VADDR_KERNEL + tl)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_CPU);
	}

	ta = syspage->idtr.addr;
	tl = syspage->idtr.limit;
	if ((page->addr >= ta - VADDR_KERNEL) && (page->addr < ta - VADDR_KERNEL + tl)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_CPU);
	}

	if ((page->addr >= syspage->pdir) && (page->addr < syspage->pdir + SIZE_PAGE)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE);
	}

	if ((page->addr >= syspage->ptable) && (page->addr < syspage->ptable + SIZE_PAGE)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE);
	}

	if ((page->addr >= ((syspage->stack - syspage->stacksize) & ~0xfff)) && (page->addr < syspage->stack)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_STACK);
	}

	if ((page->addr >= pmap_common.start) && (page->addr < pmap_common.end)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= PAGE_OWNER_KERNEL;
	}

	if ((page->addr >= syspage->kernel) && (page->addr < syspage->kernel + syspage->kernelsize)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= PAGE_OWNER_KERNEL;
 	}

	/* Check page according to loaded programs */
	for (k = 0, p = syspage->progs; k < syspage->progssz; k++, p++) {
		if (page->addr >= (p->start & ~(SIZE_PAGE - 1)) && page->addr < round_page(p->end)) {
			page->flags &= ~PAGE_FREE;
			page->flags |= PAGE_OWNER_APP;
		}
	}

	return EOK;
}


/* Function allocates page tables for kernel space */
int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp)
{
	void *vaddr;

	vaddr = (void *)((u32)(*start + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));
	if (vaddr >= end)
		return EOK;

	if (vaddr < (void *)VADDR_KERNEL)
		vaddr = (void *)VADDR_KERNEL;

	for (; vaddr < end; vaddr += (SIZE_PAGE << 10)) {
		if (pmap_enter(pmap, 0, vaddr, ~PGHD_PRESENT, NULL) < 0) {
			if (pmap_enter(pmap, 0, vaddr, ~PGHD_PRESENT, dp) < 0) {
				return -ENOMEM;
			}
			dp = NULL;
		}
		*start = vaddr;
	}

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = end;

	return EOK;
}


/* Function return character marker for page flags */
char pmap_marker(page_t *p)
{
  static const char *const marksets[4] = { "BBBBBBBBBBBBBBBB", "KYCPMSHKKKKKKKKK", "AAAAAAAAAAAAAAAA", "UUUUUUUUUUUUUUUU" };

  if (p->flags & PAGE_FREE)
    return '.';

  return marksets[(p->flags >> 1) & 3][(p->flags >> 4) & 0xf];
}


int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top)
{
	switch (i) {
	case 0:
		*vaddr = (void *)VADDR_KERNEL;
		*size = syspage->kernel;
		*prot = (PROT_WRITE | PROT_READ);
		break;
	case 1:
		*vaddr = (void *)VADDR_KERNEL + syspage->kernel;
		*size = (size_t)_etext - (size_t)*vaddr;
		*prot = (PROT_EXEC | PROT_READ);
		break;
	case 2:
		*vaddr = _etext;
		*size = (size_t)(*top) - (size_t)_etext;
		*prot = (PROT_WRITE | PROT_READ);
		break;
	default:
		return -EINVAL;
	}

	return EOK;
}


/* Function initializes low-level page mapping interface */
void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	unsigned int k;
	void *v;

	hal_spinlockCreate(&pmap_common.lock, "pmap_common.lock");

	/* Calculate physical address space range */
	pmap_common.minAddr = 0xffffffff;
	pmap_common.maxAddr = 0x00000000;

	for (k = 0; k < syspage->mmsize; k++)	{
		if (syspage->mm[k].addr < pmap_common.minAddr)
			pmap_common.minAddr = syspage->mm[k].addr;
		if (syspage->mm[k].addr + syspage->mm[k].len - 1 > pmap_common.maxAddr) {
			pmap_common.maxAddr = syspage->mm[k].addr + syspage->mm[k].len - 1;
		}
	}

	/* Initialize kernel page table - remove first 4 MB mapping */
	pmap->pdir = VADDR_KERNEL + (void *)syspage->pdir;
	pmap->pdir[0] = 0;
	pmap->cr3 = syspage->pdir;

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	hal_cpuFlushTLB(NULL);

	/* Initialize kernel heap start address */
	(*vstart) = (void *)(((u32)_end + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));
	if (*vstart < (void *)0xc00a0000)
		(*vstart) = (void *)(VADDR_KERNEL + 0x00100000);

	/* Initialize temporary page table (used for page table mapping) */
	pmap_common.ptable = (*vstart);
	(*vstart) += SIZE_PAGE;

	(*vend) = (*vstart) + SIZE_PAGE;

	pmap_common.start = (u32)pmap_common.heap - VADDR_KERNEL;
	pmap_common.end = pmap_common.start + SIZE_PAGE;

	/* Create initial heap */
	pmap_enter(pmap, pmap_common.start, (*vstart), PGHD_WRITE | PGHD_PRESENT, NULL);

	for (v = *vend; v < (void *)VADDR_KERNEL + (4 << 20); v += SIZE_PAGE)
		pmap_remove(pmap, v);

	return;
}
