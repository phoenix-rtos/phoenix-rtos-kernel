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
#include "hal/pmap.h"
#include "hal/cpu.h"
#include "hal/string.h"
#include "hal/spinlock.h"

#include "include/errno.h"
#include "include/mman.h"

#include "lib/assert.h"

#include "halsyspage.h"

#if NUM_CPUS != 1
#define hal_cpuInvalVAAll   hal_cpuInvalVA_IS
#define hal_cpuInvalASIDAll hal_cpuInvalASID_IS
#else
#define hal_cpuInvalVAAll   hal_cpuInvalVA
#define hal_cpuInvalASIDAll hal_cpuInvalASID
#endif

/* parasoft-begin-suppress MISRAC2012-RULE_8_6 "Provided by toolchain" */
extern unsigned int _end;
extern unsigned int _etext;
/* parasoft-end-suppress MISRAC2012-RULE_8_6 */

#define SIZE_EXTEND_BSS 18U * SIZE_PAGE

#define TT2S_ATTR_MASK 0xfffU
#define TT2S_NOTGLOBAL 0x800U
#define TT2S_SHAREABLE 0x400U
#define TT2S_SMALLPAGE 0x002U
#define TT2S_EXECNEVER 0x001U
/* Memory region attributes (encodes TT2 descriptor bits [11:0]: ---T EX-- CB--) */
#define TT2S_ORDERED       0x000U
#define TT2S_SHARED_DEV    0x004U
#define TT2S_CACHED        0x04cU
#define TT2S_NOTCACHED     0x040U
#define TT2S_NOTSHARED_DEV 0x080U
/* Access permission bits AP[2:0] */
#define TT2S_READONLY   0x200U
#define TT2S_PL0ACCESS  0x020U
#define TT2S_ACCESSFLAG 0x010U

#define TT2S_COMMON_ATTR  (TT2S_SMALLPAGE | TT2S_ACCESSFLAG | TT2S_SHAREABLE)
#define TT2S_CACHING_ATTR TT2S_CACHED

/* Page dirs & tables are write-back no write-allocate inner/outer cacheable, shareable */
#define TTBR_CACHE_CONF (1U | (1U << 6) | (3U << 3) | 2U)

#define ID_PDIR(vaddr)   (((ptr_t)(vaddr) >> 20))
#define ID_PTABLE(vaddr) (((ptr_t)(vaddr) >> 12) & 0x3ffU)

/* Values for PDIR entry field */
#define PDIR_TYPE_L2TABLE 0x00001U
#define PDIR_TYPE_INVALID 0x00000U

#define SCRATCH_ATTRS (PGHD_PRESENT | PGHD_READ | PGHD_WRITE)

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
	/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 MISRAC2012-RULE_1_1 "Symbol used in assembly, theres no limits" */
} __attribute__((aligned(0x4000))) pmap_common;


static const char *const marksets[4] = { "BBBBBBBBBBBBBBBB", "KYCPMSHKKKKKKKKK", "AAAAAAAAAAAAAAAA", "UUUUUUUUUUUUUUUU" };


/* clang-format off */
static const u16 attrMap[] = {
	TT2S_COMMON_ATTR | TT2S_CACHING_ATTR | TT2S_EXECNEVER | TT2S_READONLY,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV   | TT2S_EXECNEVER | TT2S_READONLY,
	TT2S_COMMON_ATTR | TT2S_CACHING_ATTR                  | TT2S_READONLY,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV                    | TT2S_READONLY,
	TT2S_COMMON_ATTR | TT2S_CACHING_ATTR | TT2S_EXECNEVER,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV   | TT2S_EXECNEVER,
	TT2S_COMMON_ATTR | TT2S_CACHING_ATTR,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV,
	TT2S_COMMON_ATTR | TT2S_CACHING_ATTR | TT2S_EXECNEVER | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV   | TT2S_EXECNEVER | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_CACHING_ATTR                  | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV                    | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_CACHING_ATTR | TT2S_EXECNEVER                 | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV   | TT2S_EXECNEVER                 | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_CACHING_ATTR                                  | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV                                    | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_NOTCACHED    | TT2S_EXECNEVER | TT2S_READONLY,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV   | TT2S_EXECNEVER | TT2S_READONLY,
	TT2S_COMMON_ATTR | TT2S_NOTCACHED                     | TT2S_READONLY,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV                    | TT2S_READONLY,
	TT2S_COMMON_ATTR | TT2S_NOTCACHED    | TT2S_EXECNEVER,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV   | TT2S_EXECNEVER,
	TT2S_COMMON_ATTR | TT2S_NOTCACHED,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV,
	TT2S_COMMON_ATTR | TT2S_NOTCACHED    | TT2S_EXECNEVER | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV   | TT2S_EXECNEVER | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_NOTCACHED                     | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV                    | TT2S_READONLY | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_NOTCACHED    | TT2S_EXECNEVER                 | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV   | TT2S_EXECNEVER                 | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_NOTCACHED                                     | TT2S_PL0ACCESS | TT2S_NOTGLOBAL,
	TT2S_COMMON_ATTR | TT2S_SHARED_DEV                                    | TT2S_PL0ACCESS | TT2S_NOTGLOBAL
};
/* clang-format on */


static void _pmap_asidAlloc(pmap_t *pmap)
{
	pmap_t *evicted;
	++pmap_common.asidptr;

	/* parasoft-suppress-next-line MISRAC2012-RULE_14_3 "asidptr may be 0 if overflow occurs" */
	while (pmap_common.asidptr == 0U || pmap_common.asid_map[pmap_common.asidptr] != NULL) {
		evicted = pmap_common.asid_map[pmap_common.asidptr];
		if (evicted != NULL) {
			if ((hal_cpuGetContextId() & 0xffU) == pmap_common.asids[evicted->asid_ix]) {
				continue;
			}
			evicted->asid_ix = 0;
			break;
		}

		++pmap_common.asidptr;
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

	if (pmap->asid_ix != 0U) {
		hal_cpuInvalASIDAll(pmap_common.asids[pmap->asid_ix]);
		if (pmap->asid_ix != pmap_common.asidptr) {
			pmap_common.asid_map[pmap->asid_ix] = last = pmap_common.asid_map[pmap_common.asidptr];
			last->asid_ix = pmap->asid_ix;
			tmp = pmap_common.asids[last->asid_ix];
			pmap_common.asids[last->asid_ix] = pmap_common.asids[pmap_common.asidptr];
			pmap_common.asids[pmap_common.asidptr] = (u8)tmp;
		}

		pmap_common.asid_map[pmap_common.asidptr] = NULL;

		if (--pmap_common.asidptr == 0U) {
			/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "underflow is expected" */
			pmap_common.asidptr--;
		}

		pmap->asid_ix = 0;
	}
}


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, const syspage_prog_t *prog, void *vaddr)
{
	pmap->pdir = vaddr;
	pmap->addr = p->addr;
	pmap->asid_ix = 0;

	hal_memset(pmap->pdir, 0, SIZE_PDIR);

	hal_cpuDataMemoryBarrier();
	hal_cpuDataSyncBarrier();
	return EOK;
}


addr_t pmap_destroy(pmap_t *pmap, unsigned int *i)
{
	spinlock_ctx_t sc;

	unsigned int max = ((VADDR_USR_MAX + SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U)) >> 20;

	hal_spinlockSet(&pmap_common.lock, &sc);
	if (pmap->asid_ix != 0U) {
		_pmap_asidDealloc(pmap);
	}
	hal_spinlockClear(&pmap_common.lock, &sc);

	while (*i < max) {
		if (pmap->pdir[*i] != 0U) {
			*i += 4U;
			return pmap->pdir[*i - 4U] & ~0xfffU;
		}
		(*i) += 4U;
	}

	return 0;
}


static void _pmap_switch(pmap_t *pmap)
{
	if (pmap->asid_ix == 0U) {
		_pmap_asidAlloc(pmap);
	}
	else if (hal_cpuGetTTBR0() == (pmap->addr | TTBR_CACHE_CONF)) {
		return;
	}
	else {
		/* No action required */
	}
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


static void _pmap_writeEntry(ptr_t *ptable, void *va, addr_t pa, vm_attr_t attr, unsigned char asid)
{
	unsigned int pti = ID_PTABLE((ptr_t)va);

	hal_cpuCleanDataCache((ptr_t)&ptable[pti], (ptr_t)&ptable[pti] + sizeof(ptr_t));
	ptr_t oldEntry = ptable[pti];
	if ((attr & PGHD_PRESENT) != 0U) {
		ptable[pti] = (pa & ~0xfffU) | attrMap[attr & 0x1fU];
	}
	else {
		ptable[pti] = 0;
	}

	hal_cpuDataSyncBarrier();
	if ((oldEntry & 0x3U) != 0U) {
		hal_cpuInvalVAAll(((ptr_t)va & ~0xfffU) | asid);
	}

	hal_cpuBranchInval();
	hal_cpuICacheInval();
}


static void _pmap_addTable(pmap_t *pmap, unsigned int pdi, addr_t pa)
{
	pa = (pa & ~0xfffU) | PDIR_TYPE_L2TABLE;

	pdi = pdi & ~3U;
	hal_cpuFlushDataCache((ptr_t)&pmap->pdir[pdi], (ptr_t)&pmap->pdir[pdi] + 4U * sizeof(ptr_t));

	/* L2 table contains 256 entries (0x400). PAGE_SIZE is equal 0x1000 so that four L2 tables are added. */
	pmap->pdir[pdi] = pa;
	pmap->pdir[pdi + 1U] = pa + 0x400U;
	pmap->pdir[pdi + 2U] = pa + 0x800U;
	pmap->pdir[pdi + 3U] = pa + 0xc00U;

	hal_cpuInvalASIDAll(pmap_common.asids[pmap->asid_ix]);
}


static void _pmap_mapScratch(addr_t pa, unsigned char asid)
{
	_pmap_writeEntry(pmap_common.kptab, pmap_common.sptab, pa, SCRATCH_ATTRS, asid);
}


/* Functions maps page at specified address */
int pmap_enter(pmap_t *pmap, addr_t paddr, void *vaddr, vm_attr_t attr, page_t *alloc)
{
	unsigned int pdi;
	unsigned char asid;
	spinlock_ctx_t sc;

	pdi = ID_PDIR((ptr_t)vaddr);

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
	_pmap_writeEntry(pmap_common.sptab, vaddr, paddr, attr, asid);

	if ((attr & PGHD_PRESENT) == 0U) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return EOK;
	}

	if ((attr & PGHD_EXEC) != 0U || (attr & PGHD_NOT_CACHED) != 0U || (attr & PGHD_DEV) != 0U) {
		/* Invalidate cache for this pa to prevent corrupting it later when cache lines get evicted.
		 * First map it into our address space if necessary. */
		if (hal_cpuGetTTBR0() != (pmap->addr | TTBR_CACHE_CONF)) {
			_pmap_mapScratch(paddr, asid);
			vaddr = pmap_common.sptab;
		}

		hal_cpuFlushDataCache((ptr_t)vaddr, (ptr_t)vaddr + SIZE_PAGE);

		if ((attr & PGHD_EXEC) != 0U) {
			hal_cpuBranchInval();
			hal_cpuICacheInval();
		}

		hal_cpuDataSyncBarrier();
		hal_cpuInstrBarrier();
	}


	hal_spinlockClear(&pmap_common.lock, &sc);
	return EOK;
}


int pmap_remove(pmap_t *pmap, void *vstart, void *vend)
{
	unsigned int pdi, pti;
	addr_t addr;
	spinlock_ctx_t sc;
	ptr_t vaddr;

	hal_spinlockSet(&pmap_common.lock, &sc);

	const u8 asid = pmap_common.asids[pmap->asid_ix];

	for (vaddr = (ptr_t)vstart; vaddr < (ptr_t)vend; vaddr += SIZE_PAGE) {
		pdi = ID_PDIR(vaddr);
		pti = ID_PTABLE(vaddr);

		addr = pmap->pdir[pdi];
		if (addr == PDIR_TYPE_INVALID) {
			continue;
		}

		/* Map page table corresponding to vaddr */
		if (pmap_common.kptab[ID_PTABLE(pmap_common.sptab)] != ((addr & ~0xfffU) | attrMap[SCRATCH_ATTRS])) {
			_pmap_mapScratch(addr, asid);
		}

		if (pmap_common.sptab[pti] == 0U) {
			continue;
		}

		_pmap_writeEntry(pmap_common.sptab, (void *)vaddr, 0x0U, 0U, asid);
	}

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
	u32 *pdir = ((ptr_t)vaddr >= VADDR_USR_MAX) ? pmap_common.kpdir : pmap->pdir;
	addr = pdir[pdi];
	if (addr == 0U) {
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

	a = *addr & ~(SIZE_PAGE - 1U);
	page->flags = 0;

	/* Test address ranges */
	hal_spinlockSet(&pmap_common.lock, &sc);
	min = pmap_common.minAddr;
	max = pmap_common.maxAddr;
	hal_spinlockClear(&pmap_common.lock, &sc);

	if (a < min) {
		a = min;
	}

	if (a >= max) {
		return -ENOMEM;
	}

	page->addr = a;
	(*addr) = a + SIZE_PAGE;

	/* TODO: Checking programs should be placed in a common part */
	prog = syspage->progs;
	if (prog != NULL) {
		do {
			if (page->addr >= prog->start && page->addr < prog->end) {
				page->flags = PAGE_OWNER_APP;
				return EOK;
			}
			prog = prog->next;
		} while (prog != syspage->progs);
	}

	if (page->addr >= min + (4U * 1024U * 1024U)) {
		page->flags = PAGE_FREE;
		return EOK;
	}

	page->flags = PAGE_OWNER_KERNEL;

	if (page->addr >= (min + (4U * 1024U * 1024U) - SIZE_PAGE)) {
		page->flags |= PAGE_KERNEL_STACK;
		return EOK;
	}

	end = ((addr_t)&_end + SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U);
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

	vaddr = (void *)(((ptr_t)*start + SIZE_PAGE - 1U) & ~((ptr_t)SIZE_PAGE - 1U));
	if ((ptr_t)vaddr >= (ptr_t)end) {
		return EOK;
	}

	if ((ptr_t)vaddr < VADDR_KERNEL) {
		vaddr = (void *)VADDR_KERNEL;
	}

	for (; (ptr_t)vaddr < (ptr_t)end; vaddr += (SIZE_PAGE << 10)) {
		if (pmap_enter(pmap, 0x0U, vaddr, ~PGHD_PRESENT, NULL) < 0) {
			if (pmap_enter(pmap, 0x0U, vaddr, ~PGHD_PRESENT, dp) < 0) {
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
	if ((p->flags & PAGE_FREE) != 0u) {
		return '.';
	}

	return marksets[(p->flags >> 1) & 3u][(p->flags >> 4) & 0xfu];
}


int pmap_segment(unsigned int i, void **vaddr, size_t *size, vm_prot_t *prot, void **top)
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
	unsigned int i;

	pmap_common.asidptr = 0;
	pmap->asid_ix = 0;

	for (i = 0; i < sizeof(pmap_common.asid_map) / sizeof(pmap_common.asid_map[0]); ++i) {
		pmap_common.asid_map[i] = NULL;
		pmap_common.asids[i] = (u8)i;
	}

	hal_spinlockCreate(&pmap_common.lock, "pmap_common.lock");

	pmap_common.minAddr = ADDR_DDR;
	pmap_common.maxAddr = ADDR_DDR + SIZE_DDR;

	/* Initialize kernel page table */
	pmap->pdir = pmap_common.kpdir;
	pmap->addr = (addr_t)pmap->pdir - VADDR_KERNEL + pmap_common.minAddr;

	/* Remove initial kernel mapping */
	for (i = 0; i < 4U; ++i) {
		pmap->pdir[ID_PDIR(pmap_common.minAddr) + i] = 0;
		hal_cpuInvalVAAll(pmap_common.minAddr + i * (1U << 2));
	}

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	/* Initialize kernel heap start address */
	(*vstart) = (void *)(((u32)&_end + SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U));

	/* First 17 pages after bss are mapped for controllers */
	/* TODO: this size should depend on platform */
	(*vstart) += SIZE_EXTEND_BSS;
	(*vend) = (*vstart) + SIZE_PAGE;

	pmap_common.start = (u32)pmap_common.heap - VADDR_KERNEL + pmap_common.minAddr;
	pmap_common.end = pmap_common.start + SIZE_PAGE;

	/* Create initial heap */
	LIB_ASSERT_ALWAYS(pmap_enter(pmap, pmap_common.start, (*vstart), PGHD_WRITE | PGHD_READ | PGHD_PRESENT, NULL) == EOK, "failed to create initial heap");

	(void)pmap_remove(pmap, *vend, (void *)(VADDR_KERNEL + (4U * 1024U * 1024U)));
}
