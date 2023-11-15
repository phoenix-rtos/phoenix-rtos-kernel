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

#include <arch/tlb.h>

#include "halsyspage.h"
#include "ia32.h"
#include "hal/pmap.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "hal/console.h"
#include "hal/tlb/tlb.h"
#include "init.h"

#include "include/errno.h"
#include "include/mman.h"


extern unsigned int _etext;


struct {
	spinlock_t lock;
} __attribute__((aligned(SIZE_PAGE))) pmap_common;


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	u32 i, pages;
	pmap->pdir = vaddr;
	pmap->cr3 = p->addr;

	/* Copy kernel page tables */
	hal_memset(pmap->pdir, 0, SIZE_PAGE);
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


int _pmap_enter(u32 *pdir, addr_t *pt, addr_t pa, void *va, int attr, page_t *alloc, int tlbInval)
{
	addr_t addr;
	addr_t *ptable;
	u32 pdi, pti;

	pdi = (u32)va >> 22;
	pti = ((u32)va >> 12) & 0x000003ffu;

	/* If no page table is allocated add new one */
	if (pdir[pdi] == 0) {
		if (alloc == NULL) {
			return -EFAULT;
		}
		pdir[pdi] = ((alloc->addr & ~(SIZE_PAGE - 1)) | PTHD_USER | PTHD_WRITE | PTHD_PRESENT);
	}

	/* Map selected page table to specified virtual address */
	addr = pdir[pdi];
	if ((u32)pt < VADDR_KERNEL) {
		return -EFAULT;
	}

	/* Map selected page table */
	ptable = (addr_t *)(syspage->hs.ptable + VADDR_KERNEL);
	ptable[((u32)pt >> 12) & 0x000003ffu] = (addr & ~(SIZE_PAGE - 1)) | (PGHD_WRITE | PGHD_PRESENT | PGHD_USER);

	hal_tlbInvalidateLocalEntry(NULL, pt);

	/* And at last map page or only changle attributes of map entry */
	pt[pti] = ((pa & ~(SIZE_PAGE - 1)) | (attr & 0xfffu) | PGHD_PRESENT);

	if (tlbInval != 0) {
		hal_tlbInvalidateEntry(NULL, va, 1);
	}
	else {
		hal_tlbInvalidateLocalEntry(NULL, va);
	}

	return EOK;
}


/* Functions maps page at specified address */
int pmap_enter(pmap_t *pmap, addr_t pa, void *va, int attr, page_t *alloc)
{
	spinlock_ctx_t sc;
	int ret;

	hal_spinlockSet(&pmap_common.lock, &sc);
	ret = _pmap_enter(pmap->pdir, hal_config.ptable, pa, va, attr, alloc, 1);
	if (ret == EOK) {
		hal_tlbCommit(&pmap_common.lock, &sc);
	}
	else {
		hal_spinlockClear(&pmap_common.lock, &sc);
	}
	return ret;
}


int _pmap_removeMany(u32 *pdir, addr_t *pt, void *vaddr, size_t count, int tlbInval)
{
	u32 pdi, pti;
	addr_t addr, *ptable;
	size_t i;
	void *va;

	ptable = (addr_t *)(syspage->hs.ptable + VADDR_KERNEL);
	va = vaddr;

	for (i = 0; i < count; ++i, va += SIZE_PAGE) {
		pdi = (u32)va >> 22;
		pti = ((u32)va >> 12) & 0x000003ffu;

		/* no page table is allocated => page is not mapped */
		if (pdir[pdi] == 0) {
			continue;
		}

		/* Map selected page table to specified virtual address */
		addr = pdir[pdi];
		if ((u32)pt < VADDR_KERNEL) {
			return -EFAULT;
		}

		/* Map selected page table */
		ptable[((u32)pt >> 12) & 0x000003ffu] = (addr & ~(SIZE_PAGE - 1)) | (PGHD_WRITE | PGHD_PRESENT);

		hal_tlbInvalidateLocalEntry(NULL, pt);

		/* Unmap page */
		pt[pti] = 0;
	}

	if (tlbInval != 0) {
		hal_tlbInvalidateEntry(NULL, vaddr, count);
	}
	else {
		for (i = 0; i < count; ++i, vaddr += SIZE_PAGE) {
			hal_tlbInvalidateLocalEntry(NULL, vaddr);
		}
	}
	return EOK;
}


static int pmap_removeMany(pmap_t *pmap, void *vaddr, size_t count)
{
	spinlock_ctx_t sc;
	int ret;
	hal_spinlockSet(&pmap_common.lock, &sc);
	ret = _pmap_removeMany(pmap->pdir, hal_config.ptable, vaddr, count, 1);
	if (ret == EOK) {
		hal_tlbCommit(&pmap_common.lock, &sc);
	}
	else {
		hal_spinlockClear(&pmap_common.lock, &sc);
	}
	return ret;
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	return pmap_removeMany(pmap, vaddr, 1);
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
	ptable[((u32)hal_config.ptable >> 12) & 0x000003ffu] = (addr & ~(SIZE_PAGE - 1)) | (PGHD_WRITE | PGHD_PRESENT);
	hal_tlbInvalidateLocalEntry(NULL, hal_config.ptable);

	addr = (addr_t)hal_config.ptable[pti];
	hal_tlbCommit(&pmap_common.lock, &sc);

	return addr;
}


int pmap_getPage(page_t *page, addr_t *addr)
{
	addr_t a;
	size_t i;

	const syspage_map_t *map;
	const mapent_t *sysEntry;
	const hal_memEntry_t *memEntry;
	const syspage_prog_t *prog;

	a = *addr & ~(SIZE_PAGE - 1);

	if (a < hal_config.minAddr) {
		a = hal_config.minAddr;
	}

	if (a >= hal_config.maxAddr) {
		return -ENOMEM;
	}

	page->addr = a;
	page->flags = 0;

	map = syspage->maps;
	if (map == NULL) {
		return -EINVAL;
	}

	for (i = 0; i < hal_config.memMap.count; ++i) {
		memEntry = &hal_config.memMap.entries[i];
		if ((a >= memEntry->start) && (a - memEntry->start < memEntry->pageCount * SIZE_PAGE)) {
			*addr = a + SIZE_PAGE;
			page->flags |= memEntry->flags;
			return EOK;
		}
	}

	do {
		if ((a >= map->start) && (a < map->end)) {
			sysEntry = map->entries;
			if (sysEntry != NULL) {
				do {
					if (((a >= sysEntry->start) && (a < sysEntry->end))) {
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
					}
					sysEntry = sysEntry->next;
				} while (sysEntry != map->entries);
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
		map = map->next;
	} while (map != syspage->maps);

	*addr = a + SIZE_PAGE;

	prog = syspage->progs;
	if (prog != NULL) {
		do {
			if ((page->addr >= prog->start) && (page->addr < prog->end)) {
				page->flags |= PAGE_OWNER_APP;
				return EOK;
			}
			prog = prog->next;
		} while (prog != syspage->progs);
	}

	page->flags |= PAGE_FREE;

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

	/* It is called only from _page_init, so there is no need for spinlocks and TLB shootdowns */
	for (; vaddr < end; vaddr += (SIZE_PAGE << 10)) {
		if (_pmap_enter(pmap->pdir, hal_config.ptable, 0, vaddr, 0, NULL, 0) < 0) {
			if (_pmap_enter(pmap->pdir, hal_config.ptable, 0, vaddr, 0, dp, 0) < 0) {
				return -ENOMEM;
			}
			dp = NULL;
		}
		*start = vaddr;
	}
	hal_tlbFlushLocal(NULL);

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
	hal_spinlockCreate(&pmap_common.lock, "pmap_common.lock");

	/* Initialize kernel page table - remove first 4 MB mapping */
	pmap->pdir = (u32 *)(VADDR_KERNEL + syspage->hs.pdir);
	pmap->pdir[0] = 0;
	pmap->cr3 = syspage->hs.pdir;

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	hal_tlbFlushLocal(NULL);

	/* Initialize kernel heap start address */
	(*vstart) = hal_config.heapStart;

	/* Map initial heap to the first physical page */
	(*vend) = (*vstart) + SIZE_PAGE;
	_pmap_enter(pmap->pdir, hal_config.ptable, 0x00000000u, (*vstart), PGHD_WRITE | PGHD_PRESENT, NULL, 0);

	/* Move heap start above BIOS Data Area */
	(*vstart) += 0x500;

	pmap_removeMany(pmap, *vend, ((void *)VADDR_KERNEL + (4 << 20) - *vend) / SIZE_PAGE);
	hal_tlbFlushLocal(NULL);

	return;
}
