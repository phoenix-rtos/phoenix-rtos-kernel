/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem (ARM)
 *
 * Copyright 2014, 2018 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "pmap.h"
#include "cpu.h"
#include "spinlock.h"
#include "string.h"

#include "../../../include/errno.h"
#include "../../../include/mman.h"

#include "syspage.h"

extern void _end(void);
extern void _etext(void);

#define TT2S_ATTR_MASK      0xfff
#define TT2S_NOTGLOBAL      0x800
#define TT2S_SHAREABLE      0x400
#define TT2S_READONLY       0x200
/* Memory region attributes (encodes TT2 descriptor bits [11:0]: ---T EX-- CB--) */
#define TT2S_ORDERED        0x000
#define TT2S_SHARED_DEV     0x004
#define TT2S_CACHED         0x00c
#define TT2S_NOTCACHED      0x040
#define TT2S_NOTSHARED_DEV  0x080
#define	TT2S_PL0ACCESS      0x020
#define TT2S_ACCESSFLAG     0x010
#define TT2S_SMALLPAGE      0x002
#define TT2S_EXECNEVER      0x001

#define TT2S_CACHING_ATTR	TT2S_CACHED


struct {
	u32 kpdir[0x1000]; /* Has to be first in the structure */
	u32 kptab[0x400];
	u32 excptab[0x400];
	u32 sptab[0x400];
	u8 heap[SIZE_PAGE];
	pmap_t *asid_map[256];
	u8 asids[256];
	addr_t minAddr;
	addr_t maxAddr;
	u32 start;
	u32 end;
	spinlock_t lock;
	u8 asidptr;
} __attribute__((aligned(0x4000))) pmap_common;


static const char *const marksets[4] = { "BBBBBBBBBBBBBBBB", "KYCPMSHKKKKKKKKK", "AAAAAAAAAAAAAAAA", "UUUUUUUUUUUUUUUU" };


static const u16 attrMap[] = {
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_CACHING_ATTR | TT2S_EXECNEVER | TT2S_READONLY,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV   | TT2S_EXECNEVER | TT2S_READONLY,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_CACHING_ATTR                  | TT2S_READONLY,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV                    | TT2S_READONLY,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_CACHING_ATTR | TT2S_EXECNEVER,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV   | TT2S_EXECNEVER,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_CACHING_ATTR,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV,
	TT2S_SMALLPAGE                   | TT2S_CACHING_ATTR | TT2S_EXECNEVER | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE                   | TT2S_SHARED_DEV   | TT2S_EXECNEVER | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE                   | TT2S_CACHING_ATTR                  | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE                   | TT2S_SHARED_DEV                    | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_CACHING_ATTR | TT2S_EXECNEVER                 | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV   | TT2S_EXECNEVER                 | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_CACHING_ATTR                                  | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV                                    | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_NOTCACHED    | TT2S_EXECNEVER | TT2S_READONLY,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV   | TT2S_EXECNEVER | TT2S_READONLY,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_NOTCACHED                     | TT2S_READONLY,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV                    | TT2S_READONLY,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_NOTCACHED    | TT2S_EXECNEVER,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV   | TT2S_EXECNEVER,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_NOTCACHED,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV,
	TT2S_SMALLPAGE                   | TT2S_NOTCACHED    | TT2S_EXECNEVER | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE                   | TT2S_SHARED_DEV   | TT2S_EXECNEVER | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE                   | TT2S_NOTCACHED                     | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE                   | TT2S_SHARED_DEV                    | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_NOTCACHED    | TT2S_EXECNEVER                 | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV   | TT2S_EXECNEVER                 | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_NOTCACHED                                     | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHARED_DEV                                    | TT2S_PL0ACCESS | TT2S_NOTGLOBAL
};


static void _pmap_asidAlloc(pmap_t *pmap)
{
	pmap_t *evicted = NULL;

	while (!++pmap_common.asidptr || (evicted = pmap_common.asid_map[pmap_common.asidptr]) != NULL) {
		if (evicted != NULL) {
			if ((hal_cpuGetContextId() & 0xff) == pmap_common.asids[evicted->asid_ix])
				continue;
			evicted->asid_ix = 0;
			break;
		}
	}

	pmap_common.asid_map[pmap_common.asidptr] = pmap;
	pmap->asid_ix = pmap_common.asidptr;
	hal_cpuInvalASID(pmap_common.asids[pmap->asid_ix]);
	hal_cpuDataSyncBarrier();
}


static void _pmap_asidDealloc(pmap_t *pmap)
{
	pmap_t *last;
	addr_t tmp;

	if (pmap->asid_ix != 0) {
		if (pmap->asid_ix != pmap_common.asidptr) {
			pmap_common.asid_map[pmap->asid_ix] = last = pmap_common.asid_map[pmap_common.asidptr];
			last->asid_ix = pmap->asid_ix;
			tmp = pmap_common.asids[last->asid_ix];
			pmap_common.asids[last->asid_ix] = pmap_common.asids[pmap_common.asidptr];
			pmap_common.asids[pmap_common.asidptr] = tmp;
		}

		pmap_common.asid_map[pmap_common.asidptr] = NULL;

		if (!--pmap_common.asidptr)
			pmap_common.asidptr--;

		pmap->asid_ix = 0;
	}
}


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	int i;

	pmap->pdir = vaddr;
	pmap->addr = p->addr;
	pmap->asid_ix = 0;

	for (i = 0; i < SIZE_PDIR / SIZE_CACHE_LINE; ++i)
		hal_cpuFlushDataCache((addr_t)pmap->pdir + i * SIZE_CACHE_LINE);

	hal_memset(pmap->pdir, 0, (VADDR_KERNEL) >> 18);
	hal_memcpy(&pmap->pdir[VADDR_KERNEL >> 20], &kpmap->pdir[VADDR_KERNEL >> 20], (VADDR_MAX - VADDR_KERNEL + 1) >> 18);

	for (i = 0; i < SIZE_PDIR / SIZE_CACHE_LINE; ++i)
		hal_cpuFlushDataCache((addr_t)pmap->pdir + i * SIZE_CACHE_LINE);

	return EOK;
}


void pmap_moved(pmap_t *pmap)
{
	hal_spinlockSet(&pmap_common.lock);
	pmap_common.asid_map[pmap->asid_ix] = pmap;
	hal_spinlockClear(&pmap_common.lock);
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	int max = ((VADDR_USR_MAX + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1)) >> 20;

	hal_spinlockSet(&pmap_common.lock);
	if (pmap->asid_ix != 0)
		_pmap_asidDealloc(pmap);
	hal_spinlockClear(&pmap_common.lock);

	while (*i < max) {
		if (pmap->pdir[*i] != NULL) {
			*i += 4;
			return pmap->pdir[*i - 4] & ~0xfff;
		}
		(*i) += 4;
	}

	return 0;
}


void pmap_switch(pmap_t *pmap)
{
	hal_spinlockSet(&pmap_common.lock);
	if (pmap->asid_ix == 0) {
		_pmap_asidAlloc(pmap);
	}
	else if (hal_cpuGetUserTT() == pmap->addr) {
		hal_spinlockClear(&pmap_common.lock);
		return;
	}

	hal_cpuSetContextId(0);
	hal_cpuSetUserTT(pmap->addr);
	hal_cpuSetContextId((u32)pmap->pdir | pmap_common.asids[pmap->asid_ix]);

	hal_cpuDataSyncBarrier();
	hal_cpuBranchInval();
	hal_cpuICacheInval();
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
	hal_spinlockClear(&pmap_common.lock);
}


static void _pmap_mapScratch(addr_t pa)
{
	addr_t *ptable = pmap_common.kptab;
	int i;

	for (i = 0; i < SIZE_PAGE / SIZE_CACHE_LINE; ++i)
		hal_cpuFlushDataCache((addr_t)pmap_common.sptab + i * SIZE_CACHE_LINE);

	hal_cpuDataSyncBarrier();
	ptable[((u32)pmap_common.sptab >> 12) & 0x3ff] = (pa & ~0xfff) | attrMap[PGHD_WRITE | PGHD_NOT_CACHED];

	hal_cpuFlushDataCache((addr_t)&ptable[((u32)pmap_common.sptab >> 12) & 0x3ff]);
	hal_cpuInvalVA((addr_t)pmap_common.sptab);
	hal_cpuDataSyncBarrier();
}


/* Functions maps page at specified address */
int pmap_enter(pmap_t *pmap, addr_t pa, void *va, int attr, page_t *alloc)
{
	unsigned int pdi, pti, i;

	pdi = (u32)va >> 20;
	pti = ((u32)va >> 12) & 0x3ff;

	hal_spinlockSet(&pmap_common.lock);

	/* If no page table is allocated add new one */
	if (!pmap->pdir[pdi]) {
		if (alloc == NULL) {
			hal_spinlockClear(&pmap_common.lock);
			return -EFAULT;
		}

		/* Clear new ptable */
		_pmap_mapScratch(alloc->addr);

		for (i = 0; i < SIZE_PAGE / SIZE_CACHE_LINE; ++i)
			hal_cpuFlushDataCache((addr_t)pmap_common.sptab + i * SIZE_CACHE_LINE);

		hal_memset(pmap_common.sptab, 0, SIZE_PAGE);

		hal_cpuDataBarrier();
		hal_cpuDataSyncBarrier();

		for (i = 0; i < SIZE_PAGE / SIZE_CACHE_LINE; ++i)
			hal_cpuFlushDataCache((addr_t)pmap_common.sptab + i * SIZE_CACHE_LINE);

		hal_cpuInstrBarrier();

		pmap->pdir[pdi & ~0x3] = ((alloc->addr & ~0xfff)) | 1;
		pmap->pdir[(pdi & ~0x3) + 1] = ((alloc->addr & ~0xfff) + 0x400) | 1;
		pmap->pdir[(pdi & ~0x3) + 2] = ((alloc->addr & ~0xfff) + 0x800) | 1;
		pmap->pdir[(pdi & ~0x3) + 3] = ((alloc->addr & ~0xfff) + 0xc00) | 1;

		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
		hal_cpuFlushDataCache((addr_t)&pmap->pdir[pdi & ~0x3]);
	}
	else {
		_pmap_mapScratch(pmap->pdir[pdi] & ~0x3ff);
	}

	hal_cpuFlushDataCache((addr_t)&pmap_common.sptab[pti]);

	if (hal_cpuGetUserTT() != pmap->addr && va < (void *)0x80000000) {
		if (attr & PGHD_PRESENT)
			pmap_common.sptab[pti] = (pa & ~0xfff) | attrMap[attr & 0x1f];
		else
			pmap_common.sptab[pti] = 0;

		hal_cpuFlushDataCache((addr_t)&pmap_common.sptab[pti]);
		hal_cpuDataBarrier();
		hal_cpuICacheInval();
		hal_cpuInstrBarrier();
		hal_spinlockClear(&pmap_common.lock);

		return EOK;
	}

	/* Map selected page table */
	hal_cpuDataSyncBarrier();

	if (pmap_common.sptab[pti] != 0) {
		/* Flush data cache */
		for (i = 0; i < SIZE_PAGE / SIZE_CACHE_LINE; ++i)
			hal_cpuFlushDataCache((addr_t)va + i * SIZE_CACHE_LINE);

		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();

		pmap_common.sptab[pti] = 0;

		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();

		hal_cpuInvalVA((addr_t)va);
	}

	/* And at last map page or only changle attributes of map entry */
	if (attr & PGHD_PRESENT)
		pmap_common.sptab[pti] = (pa & ~0xfff) | attrMap[attr & 0x1f];
	else
		pmap_common.sptab[pti] = 0;

	hal_cpuDataSyncBarrier();
	hal_cpuInvalVA((addr_t)va);
	hal_cpuFlushDataCache((addr_t)&pmap_common.sptab[pti]);
	hal_cpuBranchInval();
	hal_cpuDataSyncBarrier();
	hal_cpuDataBarrier();
	hal_cpuICacheInval();
	hal_cpuInstrBarrier();
	hal_spinlockClear(&pmap_common.lock);

	return EOK;
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	unsigned int pdi, pti, i;
	addr_t addr;

	pdi = (u32)vaddr >> 20;
	pti = ((u32)vaddr >> 12) & 0x3ff;

	hal_spinlockSet(&pmap_common.lock);
	if (!(addr = pmap->pdir[pdi])) {
		hal_spinlockClear(&pmap_common.lock);
		return EOK;
	}

	/* Map page table corresponding to vaddr at specified virtual address */
	_pmap_mapScratch(addr);

	if (pmap_common.sptab[pti] == 0) {
		hal_spinlockClear(&pmap_common.lock);
		return EOK;
	}

	if (hal_cpuGetUserTT() != pmap->addr && vaddr < (void *)0x80000000) {
		pmap_common.sptab[pti] = 0;
		hal_cpuFlushDataCache((addr_t)&pmap_common.sptab[pti]);
		hal_spinlockClear(&pmap_common.lock);
		return EOK;
	}

	/* Flush data cache */
	for (i = 0; i < SIZE_PAGE / SIZE_CACHE_LINE; ++i)
		hal_cpuFlushDataCache((addr_t)vaddr + i * SIZE_CACHE_LINE);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();

	/* Unmap page */
	pmap_common.sptab[pti] = 0;

	hal_cpuDataSyncBarrier();
	hal_cpuFlushDataCache((addr_t)&pmap_common.sptab[pti]);
	hal_cpuInstrBarrier();
	hal_cpuInvalVA((addr_t)vaddr);
	hal_spinlockClear(&pmap_common.lock);

	return EOK;
}


/* Functions returs physical address associated with specified virtual address */
addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	unsigned int pdi, pti;
	addr_t addr;

	pdi = (u32)vaddr >> 20;
	pti = ((u32)vaddr >> 12) & 0x3ff;

	hal_spinlockSet(&pmap_common.lock);
	if (!(addr = pmap->pdir[pdi])) {
		hal_spinlockClear(&pmap_common.lock);
		return 0;
	}

	/* Map page table corresponding to vaddr at specified virtual address */
	_pmap_mapScratch(addr);
	addr = (addr_t)pmap_common.sptab[pti];
	hal_spinlockClear(&pmap_common.lock);

	/* Mask out flags? */

	return addr;
}


/* Function fills page_t structure for frame given by addr */
int pmap_getPage(page_t *page, addr_t *addr)
{
	addr_t a, min, max, end;
	int i;

	a = *addr & ~(SIZE_PAGE - 1);
	page->flags = 0;

	/* Test address ranges */
	hal_spinlockSet(&pmap_common.lock);
	min = pmap_common.minAddr;
	max = pmap_common.maxAddr;
	hal_spinlockClear(&pmap_common.lock);

	if (a < min)
		a = min;

	if (a >= max)
		return -ENOMEM;

	page->addr = a;
	(*addr) = a + SIZE_PAGE;

	for (i = 0; i < syspage->progssz; ++i) {
		if (page->addr >= (addr_t)syspage->progs[i].start && page->addr < (addr_t)syspage->progs[i].end) {
			page->flags = PAGE_OWNER_APP;
			return EOK;
		}
	}

	if (page->addr >= min + (4 * 1024 * 1024)) {
		page->flags = PAGE_FREE;
		return EOK;
	}

	page->flags = PAGE_OWNER_KERNEL;

	if (page->addr >= (min + (4 * 1024 * 1024) - SIZE_PAGE)) {
		page->flags |= PAGE_KERNEL_STACK;
		return EOK;
	}

	end = ((addr_t)_end + SIZE_PAGE - 1 ) & ~(SIZE_PAGE - 1);
	if (page->addr >= end - VADDR_KERNEL + min) {
		page->flags |= PAGE_FREE;
		return EOK;
	}

	if (page->addr >= ((addr_t)pmap_common.kpdir - VADDR_KERNEL + min) && page->addr < ((addr_t)pmap_common.sptab - VADDR_KERNEL + min)) {
		page->flags |= PAGE_KERNEL_PTABLE;
		return EOK;
	}

	if (page->addr >= ((addr_t)pmap_common.sptab - VADDR_KERNEL + min) && page->addr < ((addr_t)pmap_common.sptab - VADDR_KERNEL + min + SIZE_PAGE)) {
		page->flags |= PAGE_FREE;
		return EOK;
	}

	page->flags &= ~PAGE_FREE;

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
	if (p->flags & PAGE_FREE)
		return '.';

	return marksets[(p->flags >> 1) & 3][(p->flags >> 4) & 0xf];
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


/* Function initializes low-level page mapping interface */
void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	int i;
	void *v;

	pmap_common.asidptr = 0;
	pmap->asid_ix = 0;

	for (i = 0; i < sizeof(pmap_common.asid_map) / sizeof(pmap_common.asid_map[0]); ++i) {
		pmap_common.asid_map[i] = NULL;
		pmap_common.asids[i] = i;
	}

	hal_spinlockCreate(&pmap_common.lock, "pmap_common.lock");

	pmap_common.minAddr = syspage->pbegin;
	pmap_common.maxAddr = syspage->pend;

	/* Initialize kernel page table */
	pmap->pdir = pmap_common.kpdir;
	pmap->addr = (addr_t)pmap->pdir - VADDR_KERNEL + pmap_common.minAddr;

	/* Remove initial kernel mapping */
	for (i = 0; i < 4; ++i) {
		pmap->pdir[(pmap_common.minAddr >> 20) + i] = 0;
		hal_cpuInvalVA(pmap_common.minAddr + i * (1 << 2));
	}

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	/* Initialize kernel heap start address */
	(*vstart) = (void *)(((u32)_end + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));

	/* First 9 pages after bss are mapped for UART1, UART2, GIC, GPT1, CCM and IOMUX */
	(*vstart) += 14 * SIZE_PAGE;
	(*vend) = (*vstart) + SIZE_PAGE;

	pmap_common.start = (u32)pmap_common.heap - VADDR_KERNEL + pmap_common.minAddr;
	pmap_common.end = pmap_common.start + SIZE_PAGE;

	/* Create initial heap */
	pmap_enter(pmap, pmap_common.start, (*vstart), PGHD_WRITE | PGHD_PRESENT, NULL);

	for (v = *vend; v < (void *)VADDR_KERNEL + (4 * 1024 * 1024); v += SIZE_PAGE)
		pmap_remove(pmap, v);
}
