/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem (RISCV64)
 *
 * Copyright 2018, 2020 Phoenix Systems
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
#include "dtb.h"
#include "lib/lib.h"

#include "../../include/errno.h"
#include "../../include/mman.h"


extern void _start(void);
extern void _end(void);
extern void _etext(void);


__attribute__((aligned(SIZE_PAGE)))

struct {
	/* The order of below fields should be stricly preserved */
	u64 pdir2[512];
	u64 pdir1[512];
	u64 pdir0[512];

	u64 stack[512];
	u8 heap[SIZE_PAGE];

	syspage_t pmap_syspage;

	/* The order of below fields could be randomized */
	u64 minAddr;
	u64 maxAddr;

	addr_t *ptable;

	u64 start;
	u64 end;
	spinlock_t lock;

	u64 dtb;
	u32 dtbsz;

} pmap_common;


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	unsigned int i, pages;

	pmap->pdir2 = vaddr;
	pmap->satp = (p->addr >> 12) | (u64)0x8000000000000000;

	/* Copy kernel page tables */
	hal_memset(pmap->pdir2, 0, 4096);
	vaddr = (void *)((u64)kpmap->start & ~(((u64)SIZE_PAGE << 18) - 1));
	kpmap->end = (void *)(((u64)kpmap->end + (u64)(SIZE_PAGE << 18) - 1) & ~(((u64)SIZE_PAGE << 18) - 1));

	pages = (kpmap->end - vaddr) / ((u64)SIZE_PAGE << 18);

	for (i = 0; i < pages; vaddr += (SIZE_PAGE << 18), ++i)
		pmap->pdir2[((u64)vaddr >> 30) & 0x1ff] = kpmap->pdir2[((u64)vaddr >> 30) & 0x1ff];

	return EOK;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	return 0;
}


void pmap_switch(pmap_t *pmap)
{
	hal_cpuSwitchSpace(pmap->satp);
}


/* Functions maps page at specified address (Sv39) */
int pmap_enter(pmap_t *pmap, addr_t pa, void *va, int attr, page_t *alloc)
{
	unsigned int pdi2, pdi1, pti;
	addr_t addr;
	spinlock_ctx_t sc;

	pdi2 = ((u64)va >> 30) & 0x1ff;
	pdi1 = ((u64)va >> 21) & 0x1ff;
	pti = ((u64)va >> 12) & 0x000001ff;

//	lib_printf("va=%p, pdi2=%d pdi1=%d pti=%d %p %p\n", va, pdi2, pdi1, pti, pmap->pdir2, pmap_common.ptable);

	hal_spinlockSet(&pmap_common.lock, &sc);

	/* If no page table is allocated add new one */
	if (!pmap->pdir2[pdi2]) {
		if (alloc == NULL) {
			hal_spinlockClear(&pmap_common.lock, &sc);
			return -EFAULT;
		}
		pmap->pdir2[pdi2] = (((alloc->addr >> 12) << 10) | 1);
		alloc = NULL;
	}

	/* Map next level pdir */
	addr = ((pmap->pdir2[pdi2] >> 10) << 12);

	pmap_common.pdir0[((u64)pmap_common.ptable >> 12) & 0x1ff] = (((addr >> 12) << 10) | 0xcf);

	/* PGHD_WRITE | PGHD_PRESENT | PGHD_USER); */
	hal_cpuFlushTLB(va);

	if (!pmap_common.ptable[pdi1]) {
		if (alloc == NULL) {
			hal_spinlockClear(&pmap_common.lock, &sc);
			return -EFAULT;
		}
		pmap_common.ptable[pdi1] = (((alloc->addr >> 12) << 10) | 1);
		alloc = NULL;
	}

	/* Map next level pdir */
	addr = ((pmap_common.ptable[pdi1] >> 10) << 12);

	pmap_common.pdir0[((u64)pmap_common.ptable >> 12) & 0x1ff] = (((addr >> 12) << 10) | 0xcf);

	/* PGHD_WRITE | PGHD_PRESENT | PGHD_USER); */
	hal_cpuFlushTLB(va);

	/* And at last map page or only changle attributes of map entry */
	pmap_common.ptable[pti] = (((pa >> 12) << 10) | 0xcf);

/*lib_printf("%p pdir2[%d]=%p pdir1[%d]=%p ptable[%d]=%p\n", va, pdi2, a2, pdi1, a1, pti, pa);*/

	hal_cpuFlushTLB(va);

	hal_spinlockClear(&pmap_common.lock, &sc);

	return EOK;

}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	return EOK;
}


/* Functions returs physical address associated with specified virtual address */
addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	return 0;
}


/* Function fills page_t structure for frame given by addr */
int pmap_getPage(page_t *page, addr_t *addr)
{
	addr_t a;
	spinlock_ctx_t sc;

	a = *addr & ~0xfff;
	page->flags = 0;

	/* Test address ranges */
	hal_spinlockSet(&pmap_common.lock, &sc);

	/* Ignore bbl area */
	if (((a >= 0x80000000) && (a < 0x80200000)) || (a < pmap_common.minAddr))
		a = 0x80200000;

	if (a >= pmap_common.maxAddr) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return -ENOMEM;
	}

	hal_spinlockClear(&pmap_common.lock, &sc);

	page->addr = a;
	page->flags = 0;

	if ((page->addr >= syspage->kernel) && (page->addr < syspage->kernel + syspage->kernelsize)) {
		page->flags |= PAGE_OWNER_KERNEL;

		if ((page->addr >= syspage->pdir2) && (page->addr < syspage->pdir2 + 3 * SIZE_PAGE))
			page->flags |= PAGE_KERNEL_PTABLE;

		if ((page->addr >= syspage->stack) && (page->addr < syspage->stack + syspage->stacksz))
			page->flags |= PAGE_KERNEL_STACK;
	}
	else if ((page->addr >= pmap_common.dtb) && (page->addr < pmap_common.dtb + pmap_common.dtbsz))
		page->flags |= PAGE_OWNER_BOOT;

	else
		page->flags |= PAGE_FREE;

	*addr = a + SIZE_PAGE;

	return EOK;
}


/* Function allocates page tables for kernel space */
int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp)
{
	void *vaddr;

	vaddr = (void *)((u64)(*start + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));

	if (vaddr >= end)
		return EOK;

	if (vaddr < (void *)VADDR_KERNEL)
		vaddr = (void *)VADDR_KERNEL;

	for (; vaddr < end; vaddr += ((u64)SIZE_PAGE << 30)) {
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
	char *marksets[4] = { "BBBBBBBBBBBBBBBB", "KYCPMSHKKKKKKKKK", "AAAAAAAAAAAAAAAA", "UUUUUUUUUUUUUUUU" };

	if (p->flags & PAGE_FREE)
		return '.';

	return marksets[(p->flags >> 1) & 3][(p->flags >> 4) & 0xf];
}


/* Function initializes low-level page mapping interface */
void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	void *v;
	struct {
		u64 addr;
		u64 limit;
	} *m, *r;
	size_t n, i;
	u64 a, l;
	u64 e = (u64)_end;

	dtb_getMemory((u64 **)&m, &n);
	dtb_getReservedMemory((u64 **)&r);
	dtb_getDTBArea(&pmap_common.dtb, &pmap_common.dtbsz);

	e += pmap_common.dtbsz;

	hal_spinlockCreate(&pmap_common.lock, "pmap_common.lock");

	/* Calculate physical address space range */
	pmap_common.minAddr = (u64)-1;
	pmap_common.maxAddr = 0;

	for (i = 0; i < n; i++) {
		a = ntoh64(m[i].addr);
		l = ntoh64(m[i].limit);

		if (a + l > pmap_common.maxAddr)
			pmap_common.maxAddr = a + l;
		if (a < pmap_common.minAddr)
			pmap_common.minAddr = a;
	}

	/* Initialize kernel page table - remove first 4 MB mapping */
	pmap->pdir2 = pmap_common.pdir2;
	pmap->satp = ((syspage->pdir2 >> 12) | (u64)0x8000000000000000);

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	/* Initialize kernel heap start address */
	(*vstart) = (void *)((e + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));

	/* Initialize temporary page table (used for page table mapping) */
	pmap_common.ptable = (*vstart);
	(*vstart) += SIZE_PAGE;

	(*vend) = (*vstart) + SIZE_PAGE;

	pmap_common.start = (u64)pmap_common.heap - VADDR_KERNEL + syspage->kernel;
	pmap_common.end = pmap_common.start + SIZE_PAGE;

	/* Create initial heap */
	pmap_enter(pmap, pmap_common.start, (*vstart), PGHD_WRITE | PGHD_PRESENT, NULL);

	for (v = *vend; v < (void *)VADDR_KERNEL + (4 << 20); v += SIZE_PAGE)
		pmap_remove(pmap, v);

	hal_cpuFlushTLB(NULL);

	return;
}


int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top)
{
	switch (i) {
	case 0:
		*vaddr = (void *)VADDR_KERNEL;
		*size = (size_t)_etext - VADDR_KERNEL;
		*prot = (PROT_EXEC | PROT_READ);
		break;
	case 1:
		*vaddr = _etext;
		*size = (size_t)(*top) - (size_t)_etext;
		*prot = (PROT_WRITE | PROT_READ);
		break;
	default:
		return -EINVAL;
	}

	return EOK;
}


void _pmap_preinit(void)
{
	unsigned int i;

	/* Initialize syspage with zeros */
	hal_memset(&pmap_common.pmap_syspage, 0, sizeof(syspage_t));

	syspage->kernel = (u64)_start;
	syspage->kernelsize = (u64)_end - (u64)_start;

	syspage->pdir2 = (u64)pmap_common.pdir2;
	syspage->stack = (u64)pmap_common.stack;
	syspage->stacksz = SIZE_PAGE;

	/* pmap_common.pdir2[(VADDR_KERNEL >> 30) % 512] = ((((u64)_start >> 30) << 28) | 0xcf); */

	hal_memset(pmap_common.pdir2, 0, SIZE_PAGE);

	/* Map 4MB after _start symbol at VADDR_KERNEL */
	pmap_common.pdir2[(VADDR_KERNEL >> 30) % 512] = ((u64)pmap_common.pdir1 >> 2) | 1;
	pmap_common.pdir1[(VADDR_KERNEL >> 21) % 512] = ((u64)pmap_common.pdir0 >> 2) | 1;

	for (i = 0; i < 512; i++)
		pmap_common.pdir0[((VADDR_KERNEL >> 12) % 512) + i] = ((( ((u64)_start + i * SIZE_PAGE) >> 12) << 10) | 0xcf);

	return;
}
