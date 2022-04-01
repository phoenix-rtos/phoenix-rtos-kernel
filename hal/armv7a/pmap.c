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

#include "armv7a.h"
#include "../pmap.h"
#include "../cpu.h"
#include "../string.h"
#include "../spinlock.h"

#include "../../include/errno.h"
#include "../../include/mman.h"

#include "halsyspage.h"

extern unsigned int _end;
extern unsigned int _etext;

#define SIZE_EXTEND_BSS 18 * SIZE_PAGE

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

/* Page dirs & tables are write-back no write-allocate inner/outer cachable */
#define TTBR_CACHE_CONF (1 | (1 << 6) | (3 << 3))

#define ID_PDIR(vaddr)   (((ptr_t)(vaddr) >> 20))
#define ID_PTABLE(vaddr) (((ptr_t)(vaddr) >> 12) & 0x3ff)

/* Values for PDIR entry field */
#define PDIR_TYPE_L2TABLE 0x00001
#define PDIR_TYPE_INVALID 0x00000


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
	hal_cpuInstrBarrier();
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
	pmap->pdir = vaddr;
	pmap->addr = p->addr;
	pmap->asid_ix = 0;

	hal_memset(pmap->pdir, 0, (VADDR_KERNEL) >> 18);
	hal_memcpy(&pmap->pdir[ID_PDIR(VADDR_KERNEL)], &kpmap->pdir[ID_PDIR(VADDR_KERNEL)], (VADDR_MAX - VADDR_KERNEL + 1) >> 18);

	hal_cpuDataMemoryBarrier();
	hal_cpuDataSyncBarrier();
	return EOK;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	spinlock_ctx_t sc;

	int max = ((VADDR_USR_MAX + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1)) >> 20;

	hal_spinlockSet(&pmap_common.lock, &sc);
	if (pmap->asid_ix != 0)
		_pmap_asidDealloc(pmap);
	hal_spinlockClear(&pmap_common.lock, &sc);

	while (*i < max) {
		if (pmap->pdir[*i] != NULL) {
			*i += 4;
			return pmap->pdir[*i - 4] & ~0xfff;
		}
		(*i) += 4;
	}

	return 0;
}


void _pmap_switch(pmap_t *pmap)
{
	if (pmap->asid_ix == 0)
		_pmap_asidAlloc(pmap);
	else if (hal_cpuGetTTBR0() == (pmap->addr | TTBR_CACHE_CONF))
		return;

	/* Assign new user's page dir to TTBR0 register */
	hal_cpuDataSyncBarrier();
	hal_cpuSetContextId(0);
	hal_cpuInstrBarrier();
	hal_cpuSetTTBR0(pmap->addr | TTBR_CACHE_CONF);
	hal_cpuInstrBarrier();
	hal_cpuSetContextId((u32)pmap->pdir | pmap_common.asids[pmap->asid_ix]);

	/* TODO: invalidate TLB only if asids pool is run out.
       This code should be moved to _pmap_asidAlloc and _pmap_asidDealloc */
	hal_cpuInvalTLB();

	hal_cpuBranchInval();
	hal_cpuICacheInval();
}


void pmap_switch(pmap_t *pmap)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);
	_pmap_switch(pmap);
	hal_spinlockClear(&pmap_common.lock, &sc);
}


static void _pmap_writeEntry(ptr_t *ptable, void *va, addr_t pa, int attributes, unsigned char asid)
{
	int pti = ID_PTABLE((ptr_t)va);

	hal_cpuCleanDataCache((ptr_t)&ptable[pti], (ptr_t)&ptable[pti] + sizeof(ptr_t));
	if (attributes & PGHD_PRESENT)
		ptable[pti] = (pa & ~0xfff) | attrMap[attributes & 0x1f];
	else
		ptable[pti] = 0;

	hal_cpuDataSyncBarrier();
	hal_cpuInvalVA(((ptr_t)va & ~0xfff) | asid);

	hal_cpuBranchInval();
	hal_cpuICacheInval();
}


static void _pmap_addTable(pmap_t *pmap, int pdi, addr_t pa)
{
	pa = (pa & ~0xfff) | PDIR_TYPE_L2TABLE;

	pdi &= ~3;
	hal_cpuFlushDataCache((ptr_t)&pmap->pdir[pdi], (ptr_t)&pmap->pdir[pdi] + 4 * sizeof(ptr_t));

	/* L2 table contains 256 entries (0x400). PAGE_SIZE is equal 0x1000 so that four L2 tables are added. */
	pmap->pdir[pdi]     = pa;
	pmap->pdir[pdi + 1] = pa + 0x400;
	pmap->pdir[pdi + 2] = pa + 0x800;
	pmap->pdir[pdi + 3] = pa + 0xc00;

	hal_cpuInvalASID(pmap_common.asids[pmap->asid_ix]);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


static void _pmap_mapScratch(addr_t pa, unsigned char asid)
{
	_pmap_writeEntry(pmap_common.kptab, pmap_common.sptab, pa, PGHD_PRESENT | PGHD_READ | PGHD_WRITE, asid);
}


/* Functions maps page at specified address */
int pmap_enter(pmap_t *pmap, addr_t pa, void *va, int attr, page_t *alloc)
{
	int pdi;
	unsigned char asid;
	spinlock_ctx_t sc;

	pdi = ID_PDIR((ptr_t)va);

	hal_spinlockSet(&pmap_common.lock, &sc);
	asid = pmap_common.asids[pmap->asid_ix];

	/* If no page table is allocated add new one */
	if (pmap->pdir[pdi] == PDIR_TYPE_INVALID) {
		if (alloc == NULL) {
			hal_spinlockClear(&pmap_common.lock, &sc);
			return -EFAULT;
		}

		_pmap_mapScratch(alloc->addr, asid);

		hal_cpuFlushDataCache((ptr_t)pmap_common.sptab, (ptr_t)pmap_common.sptab + SIZE_PAGE);
		hal_memset(pmap_common.sptab, 0, SIZE_PAGE);

		_pmap_addTable(pmap, pdi, alloc->addr);
	}
	else {
		_pmap_mapScratch(pmap->pdir[pdi], asid);
	}


	/* Write entry into page table */
	_pmap_writeEntry(pmap_common.sptab, va, pa, attr, asid);

	if (!(attr & PGHD_PRESENT)) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return EOK;
	}

	if (attr & PGHD_EXEC || attr & PGHD_NOT_CACHED || attr & PGHD_DEV) {
		/* Invalidate cache for this pa to prevent corrupting it later when cache lines get evicted.
		 * First map it into our address space if necessary. */
		if (hal_cpuGetTTBR0() != (pmap->addr | TTBR_CACHE_CONF)) {
			_pmap_mapScratch(pa, asid);
			va = pmap_common.sptab;
		}

		hal_cpuFlushDataCache((ptr_t)va, (ptr_t)va + SIZE_PAGE);

		if (attr & PGHD_EXEC) {
			hal_cpuBranchInval();
			hal_cpuICacheInval();
		}

		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
	}


	hal_spinlockClear(&pmap_common.lock, &sc);
	return EOK;
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	unsigned int pdi, pti;
	addr_t addr;
	unsigned char asid;
	spinlock_ctx_t sc;

	pdi = ID_PDIR((ptr_t)vaddr);
	pti = ID_PTABLE((ptr_t)vaddr);

	hal_spinlockSet(&pmap_common.lock, &sc);
	if (!(addr = pmap->pdir[pdi])) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return EOK;
	}

	/* Map page table corresponding to vaddr */
	asid = pmap_common.asids[pmap->asid_ix];
	_pmap_mapScratch(addr, asid);

	if (pmap_common.sptab[pti] == 0) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return EOK;
	}

	_pmap_writeEntry(pmap_common.sptab, vaddr, 0, 0, asid);

	hal_spinlockClear(&pmap_common.lock, &sc);

	return EOK;
}


/* Functions returns physical address associated with specified virtual address */
addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	unsigned int pdi, pti;
	addr_t addr;
	spinlock_ctx_t sc;
	unsigned char asid;

	pdi = ID_PDIR((ptr_t)vaddr);
	pti = ID_PTABLE((ptr_t)vaddr);


	hal_spinlockSet(&pmap_common.lock, &sc);
	if (!(addr = pmap->pdir[pdi])) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return 0;
	}

	asid = pmap_common.asids[pmap->asid_ix];
	_pmap_mapScratch(addr, asid);
	addr = pmap_common.sptab[pti];
	hal_spinlockClear(&pmap_common.lock, &sc);

	/* Mask out flags? */
	return addr;
}


/* Function fills page_t structure for frame given by addr */
int pmap_getPage(page_t *page, addr_t *addr)
{
	addr_t a, min, max, end;
	spinlock_ctx_t sc;

	const syspage_prog_t *prog;

	a = *addr & ~(SIZE_PAGE - 1);
	page->flags = 0;

	/* Test address ranges */
	hal_spinlockSet(&pmap_common.lock, &sc);
	min = pmap_common.minAddr;
	max = pmap_common.maxAddr;
	hal_spinlockClear(&pmap_common.lock, &sc);

	if (a < min)
		a = min;

	if (a >= max)
		return -ENOMEM;

	page->addr = a;
	(*addr) = a + SIZE_PAGE;

	/* TODO: Checking programs should be placed in a common part */
	if ((prog = syspage->progs) != NULL) {
		do {
			if (page->addr >= prog->start && page->addr < prog->end) {
				page->flags = PAGE_OWNER_APP;
				return EOK;
			}
		} while ((prog = prog->next) != syspage->progs);
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

	end = ((addr_t)&_end + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1);
	end += SIZE_EXTEND_BSS;
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
			*size = (size_t)&_etext - VADDR_KERNEL;
			*prot = (PROT_EXEC | PROT_READ);
			break;
		case 1:
			*vaddr = &_etext;
			*size = (size_t)(*top) - (size_t)&_etext;
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

	pmap_common.minAddr = ADDR_DDR;
	pmap_common.maxAddr = ADDR_DDR + SIZE_DDR;

	/* Initialize kernel page table */
	pmap->pdir = pmap_common.kpdir;
	pmap->addr = (addr_t)pmap->pdir - VADDR_KERNEL + pmap_common.minAddr;

	/* Remove initial kernel mapping */
	for (i = 0; i < 4; ++i) {
		pmap->pdir[ID_PDIR(pmap_common.minAddr) + i] = 0;
		hal_cpuInvalVA(pmap_common.minAddr + i * (1 << 2));
	}

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	/* Initialize kernel heap start address */
	(*vstart) = (void *)(((u32)&_end + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));

	/* First 17 pages after bss are mapped for controllers */
	/* TODO: this size should depend on platform */
	(*vstart) += SIZE_EXTEND_BSS;
	(*vend) = (*vstart) + SIZE_PAGE;

	pmap_common.start = (u32)pmap_common.heap - VADDR_KERNEL + pmap_common.minAddr;
	pmap_common.end = pmap_common.start + SIZE_PAGE;

	/* Create initial heap */
	pmap_enter(pmap, pmap_common.start, (*vstart), PGHD_WRITE | PGHD_READ | PGHD_PRESENT, NULL);

	for (v = *vend; v < (void *)VADDR_KERNEL + (4 * 1024 * 1024); v += SIZE_PAGE)
		pmap_remove(pmap, v);
}
