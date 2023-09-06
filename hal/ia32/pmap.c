/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem
 *
 * Copyright 2012, 2016, 2021 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "halsyspage.h"
#include "ia32.h"
#include "../pmap.h"
#include "../spinlock.h"
#include "../string.h"
#include "../console.h"
#include "tlb.h"

#include "../../include/errno.h"
#include "../../include/mman.h"


extern unsigned int _end;
extern unsigned int _etext;


struct {
	u32 minAddr;
	u32 maxAddr;
	addr_t *ptable;
	u32 start;
	u32 end;
	spinlock_t lock;
	addr_t ebda;
} __attribute__((aligned(SIZE_PAGE))) pmap_common;


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	u32 i, pages;
	pmap->pdir = vaddr;
	pmap->cr3 = p->addr;

	/* Copy kernel page tables */
	hal_memset(pmap->pdir, 0, 4096);
	vaddr = (void *)((VADDR_KERNEL + SIZE_PAGE) & ~(SIZE_PAGE - 1));

	pages = ((void *)0xffffffffu - vaddr) / (SIZE_PAGE << 10);
	for (i = 0; i < pages; vaddr += (SIZE_PAGE << 10), ++i) {
		pmap->pdir[(u32) vaddr >> 22] = kpmap->pdir[(u32) vaddr >> 22];
	}

	return EOK;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	int kernel = ((VADDR_KERNEL + SIZE_PAGE) & ~(SIZE_PAGE - 1)) >> 22;

	while (*i < kernel) {
		if (pmap->pdir[*i] != NULL) {
			return pmap->pdir[(*i)++] & ~(SIZE_PAGE - 1);
		}
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
	u32 pdi, pti;
	addr_t addr;
	addr_t *ptable;
	spinlock_ctx_t sc;

	pdi = (u32)va >> 22;
	pti = ((u32)va >> 12) & 0x000003ffu;

	/* If no page table is allocated add new one */
	if (pmap->pdir[pdi] == 0) {
		if (alloc == NULL) {
			return -EFAULT;
		}
		pmap->pdir[pdi] = ((alloc->addr & ~(SIZE_PAGE - 1)) | /*(attr & 0xfff) |*/ PTHD_USER | PTHD_WRITE | PTHD_PRESENT);
	}

	hal_spinlockSet(&pmap_common.lock, &sc);

	/* Map selected page table to specified virtual address */
	addr = pmap->pdir[pdi];
	if ((u32)pmap_common.ptable < VADDR_KERNEL) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return -EFAULT;
	}

	/* Map selected page table */
	ptable = (addr_t *)(syspage->hs.ptable + VADDR_KERNEL);
	ptable[((u32)pmap_common.ptable >> 12) & 0x000003ffu] = (addr & ~(SIZE_PAGE - 1)) | (PGHD_WRITE | PGHD_PRESENT | PGHD_USER);

	hal_tlbInvalidateEntry(pmap_common.ptable);

	/* And at last map page or only changle attributes of map entry */
	pmap_common.ptable[pti] = ((pa & ~(SIZE_PAGE - 1)) | (attr & 0xfffu) | PGHD_PRESENT);

	hal_tlbInvalidateEntry(va);
	hal_tlbCommit(&pmap_common.lock, &sc);

	return EOK;
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	u32 pdi, pti;
	addr_t addr, *ptable;
	spinlock_ctx_t sc;

	pdi = (u32)vaddr >> 22;
	pti = ((u32)vaddr >> 12) & 0x000003ffu;

	/* no page table is allocated => page is not mapped */
	if (pmap->pdir[pdi] == 0) {
		return EOK;
	}

	hal_spinlockSet(&pmap_common.lock, &sc);

	/* Map selected page table to specified virtual address */
	addr = pmap->pdir[pdi];
	if ((u32)pmap_common.ptable < VADDR_KERNEL) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return -EFAULT;
	}

	/* Map selected page table */
	ptable = (addr_t *)(syspage->hs.ptable + VADDR_KERNEL);
	ptable[((u32)pmap_common.ptable >> 12) & 0x000003ffu] = (addr & ~(SIZE_PAGE - 1)) | (PGHD_WRITE | PGHD_PRESENT);

	hal_tlbInvalidateEntry(pmap_common.ptable);

	/* Unmap page */
	pmap_common.ptable[pti] = 0;

	hal_tlbInvalidateEntry(vaddr);
	hal_tlbCommit(&pmap_common.lock, &sc);

	return EOK;
}


/* Functions returs physical address associated with specified virtual address */
addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	u32 pdi, pti;
	addr_t addr, *ptable;
	spinlock_ctx_t sc;

	pdi = (u32)vaddr >> 22;
	pti = ((u32)vaddr >> 12) & 0x000003ffu;

	if (pmap->pdir[pdi] == 0) {
		return 0;
	}

	hal_spinlockSet(&pmap_common.lock, &sc);

	/* Map page table corresponding to vaddr at specified virtual address */
	addr = pmap->pdir[pdi];

	ptable = (addr_t *)(syspage->hs.ptable + VADDR_KERNEL);
	ptable[((u32)pmap_common.ptable >> 12) & 0x000003ffu] = (addr & ~(SIZE_PAGE - 1)) | (PGHD_WRITE | PGHD_PRESENT);
	hal_tlbInvalidateEntry(pmap_common.ptable);

	addr = (addr_t)pmap_common.ptable[pti];
	hal_tlbCommit(&pmap_common.lock, &sc);

	/*if (((*paddr) & PGHD_PRESENT) == 0)
		return -ENOMEM;

	*paddr = ((*paddr) & ~0xfff) | ((u32)vaddr & 0xfff);*/

	return addr;
}


/* Function fills page_t structure for frame given by addr */
int pmap_getPage(page_t *page, addr_t *addr)
{
	addr_t a;
	size_t sz;
	spinlock_ctx_t sc;

	const syspage_map_t *map;
	const mapent_t *sysEntry;
	const syspage_prog_t *prog;

	a = *addr & ~(SIZE_PAGE - 1);

	/* Test address ranges */
	hal_spinlockSet(&pmap_common.lock, &sc);

	if (a < pmap_common.minAddr)
		a = pmap_common.minAddr;

	if (a >= pmap_common.maxAddr) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return -ENOMEM;
	}

	hal_spinlockClear(&pmap_common.lock, &sc);

	page->addr = a;
	page->flags = 0;

	map = syspage->maps;
	if (map == NULL) {
		return -EINVAL;
	}

	do {
		if (a >= map->start && a < map->end) {
			sysEntry = map->entries;
			if (sysEntry != NULL) {
				do {
					if (((a >= sysEntry->start) && (a < sysEntry->end)) == 0) {
						continue;
					}

					/* Memory reserved for boot rom */
					if ((sysEntry->type == hal_entryReserved)) {
						*addr = a + SIZE_PAGE;
						page->flags |= PAGE_OWNER_BOOT;
						return EOK;
					}

					/* Skip invalid entries in map */
					if (sysEntry->type == hal_entryInvalid) {
						a = (sysEntry->end & ~(SIZE_PAGE - 1)) - SIZE_PAGE;
						*addr = a + SIZE_PAGE;
						return -EINVAL;
					}
				} while ((sysEntry = sysEntry->next) != map->entries);
			}
		}
		else {
			/* Skip empty area between maps */
			if ((a > (map->end - 1)) && (a < map->next->start)) {
				a = (map->next->start & ~(SIZE_PAGE - 1)) - SIZE_PAGE;
				*addr = a + SIZE_PAGE;
				return -EINVAL;
			}
		}
	} while ((map = map->next) != syspage->maps);

	page->flags |= PAGE_FREE;
	*addr = a + SIZE_PAGE;


	if ((page->addr >= (ptr_t)syspage - VADDR_KERNEL) && (page->addr < (ptr_t)syspage - VADDR_KERNEL + syspage->size)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_SYSPAGE);
	}

	/* Check addr according GDT parameters */
	if ((page->addr >= syspage->hs.gdtr.addr - VADDR_KERNEL) && (page->addr < syspage->hs.gdtr.addr - VADDR_KERNEL + syspage->hs.gdtr.size)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_CPU);
	}

	if ((page->addr >= syspage->hs.idtr.addr - VADDR_KERNEL) && (page->addr < syspage->hs.idtr.addr - VADDR_KERNEL + syspage->hs.idtr.size)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_CPU);
	}

	if ((page->addr >= syspage->hs.pdir) && (page->addr < syspage->hs.pdir + SIZE_PAGE)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE);
	}

	if ((page->addr >= syspage->hs.ptable) && (page->addr < syspage->hs.ptable + SIZE_PAGE)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE);
	}

	if ((page->addr >= ((syspage->hs.stack - syspage->hs.stacksz) & ~(SIZE_PAGE - 1))) && (page->addr < syspage->hs.stack)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= (PAGE_OWNER_KERNEL | PAGE_KERNEL_STACK);
	}

	/* BIOS Data Area with initial heap */
	if ((page->addr >= pmap_common.start) && (page->addr < pmap_common.end)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= PAGE_OWNER_KERNEL;
	}

	/* Extended BIOS Data Area */
	if ((page->addr >= pmap_common.ebda) && (page->addr <= 0x0009ffff)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= PAGE_OWNER_BOOT;
	}

	sz = (size_t)((ptr_t)&_end - (ptr_t)(VADDR_KERNEL + syspage->pkernel));
	if ((page->addr >= syspage->pkernel) && (page->addr < syspage->pkernel + sz)) {
		page->flags &= ~PAGE_FREE;
		page->flags |= PAGE_OWNER_KERNEL;
	}

	prog = syspage->progs;
	if (prog != NULL) {
		do {
			if (page->addr >= prog->start && page->addr < prog->end) {
				page->flags &= ~PAGE_FREE;
				page->flags |= PAGE_OWNER_APP;
				return EOK;
			}
		} while ((prog = prog->next) != syspage->progs);
	}

	return EOK;
}


/* Function allocates page tables for kernel space */
int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp)
{
	void *vaddr;

	vaddr = (void *)((u32)(*start + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));
	if (vaddr >= end) {
		return EOK;
	}

	if (vaddr < (void *)VADDR_KERNEL) {
		vaddr = (void *)VADDR_KERNEL;
	}

	for (; vaddr < end; vaddr += (SIZE_PAGE << 10)) {
		if (pmap_enter(pmap, 0, vaddr, 0, NULL) < 0) {
			if (pmap_enter(pmap, 0, vaddr, 0, dp) < 0) {
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

	if ((p->flags & PAGE_FREE) != 0) {
		return '.';
	}

	return marksets[(p->flags >> 1) & 3][(p->flags >> 4) & 0xf];
}


int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top)
{
	switch (i) {
		case 0:
			*vaddr = (void *)VADDR_KERNEL;
			*size = syspage->pkernel;
			*prot = (PROT_WRITE | PROT_READ);
			break;
		case 1:
			*vaddr = (void *)(VADDR_KERNEL + syspage->pkernel);
			*size = (ptr_t)&_etext - (ptr_t)*vaddr;
			*prot = (PROT_EXEC | PROT_READ);
			break;
		case 2:
			*vaddr = &_etext;
			*size = (ptr_t)(*top) - (ptr_t)&_etext;
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
	void *v;
	const syspage_map_t *map;

	hal_spinlockCreate(&pmap_common.lock, "pmap_common.lock");

	/* Calculate physical address space range */
	pmap_common.minAddr = 0xffffffff;
	pmap_common.maxAddr = 0x00000000;

	map = syspage->maps;
	if (map == NULL) {
		return;
	}

	do {
		if (map->start < pmap_common.minAddr) {
			pmap_common.minAddr = map->start;
		}

		if (map->end > pmap_common.maxAddr) {
			pmap_common.maxAddr = map->end;
		}
	} while ((map = map->next) != syspage->maps);

	/* Initialize kernel page table - remove first 4 MB mapping */
	pmap->pdir = (u32 *)(VADDR_KERNEL + syspage->hs.pdir);
	pmap->pdir[0] = 0;
	pmap->cr3 = syspage->hs.pdir;

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	hal_tlbFlushLocal();

	/* Initialize kernel heap start address */
	(*vstart) = (void *)(((ptr_t)&_end + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));
	if (*vstart < (void *)0xc00a0000) {
		(*vstart) = (void *)(VADDR_KERNEL + 0x00100000);
	}

	/* Initialize temporary page table (used for page table mapping) */
	pmap_common.ptable = (*vstart);
	(*vstart) += SIZE_PAGE;

	/* Map initial heap to first physical page */
	pmap_common.start = 0x00000000;
	pmap_common.end = pmap_common.start + SIZE_PAGE;
	(*vend) = (*vstart) + SIZE_PAGE;
	pmap_enter(pmap, pmap_common.start, (*vstart), PGHD_WRITE | PGHD_PRESENT, NULL);

	/* Move heap start above BIOS Data Area */
	(*vstart) += 0x500;

	/* Initialize Extended BIOS Data Area start address */
	pmap_common.ebda = (*(u16 *)(VADDR_KERNEL + 0x40e) << 4) & ~(SIZE_PAGE - 1);
	if ((pmap_common.ebda < 0x00080000) || (pmap_common.ebda > 0x0009ffff)) {
		pmap_common.ebda = 0x00080000;
		hal_consolePrint(ATTR_NORMAL, "vm: EBDA address not defined, setting to default (0x80000)\n");
	}

	for (v = *vend; v < (void *)VADDR_KERNEL + (4 << 20); v += SIZE_PAGE) {
		pmap_remove(pmap, v);
	}
	hal_tlbFlushLocal();

	return;
}
