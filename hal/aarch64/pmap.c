/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap interface - machine dependent part of VM subsystem (AArch64)
 *
 * Copyright 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Julia Kosowska, Lukasz Leczkowski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "aarch64.h"
#include "hal/pmap.h"
#include "hal/cpu.h"
#include "hal/string.h"
#include "hal/spinlock.h"

#include "include/errno.h"
#include "include/mman.h"

#include "halsyspage.h"
#include "dtb.h"

extern unsigned int _start;
extern unsigned int _end;
extern unsigned int _etext;

typedef u64 descr_t;

/* Descriptor bitfields */
#define DESCR_VALID     (1uL << 0)         /* Descriptor is valid */
#define DESCR_TABLE     (1uL << 1)         /* Page or table descriptor */
#define DESCR_ATTR(x)   (((x) & 0x7) << 2) /* Memory attribute from MAIR_EL1 */
#define DESCR_AP1       (1uL << 6)         /* Unprivileged access */
#define DESCR_AP2       (1uL << 7)         /* Read only */
#define DESCR_nSH       (0uL << 8)         /* Non-shareable */
#define DESCR_OSH       (2uL << 8)         /* Outer shareable */
#define DESCR_ISH       (3uL << 8)         /* Inner shareable */
#define DESCR_AF        (1uL << 10)        /* Access flag */
#define DESCR_nG        (1uL << 11)        /* Not global */
#define DESCR_UXN       (1uL << 54)        /* Unprivileged execute-never */
#define DESCR_PXN       (1uL << 53)        /* Privileged execute-never */
#define DESCR_PA(entry) ((entry) & ((1uL << 48) - (1uL << 12)))

#define ATTR_FROM_DESCR(entry) (((entry) >> 2) & 0x7)

/* MAIR register bitfields */
#define MAIR_ATTR(idx, val)       (((u64)val) << (idx * 8))
#define MAIR_DEVICE(type)         (((type) & 0x3) << 2)
#define MAIR_NORMAL(inner, outer) (((inner) & 0xf) | (((outer) & 0xf) << 4))
#define MAIR_DEV_nGnRnE           0x0 /* Gathering, re-ordering, early write acknowledge all disabled */
#define MAIR_DEV_nGnRE            0x1 /* Gathering, re-ordering disabled, early write acknowledge enabled */
#define MAIR_NOR_NC               0x4 /* Non-cacheable */
#define MAIR_NOR_C_WB_NT_RA_WA    0xf /* Cacheable, write-back, non-transient, read-allocate, write-allocate */

#define MAIR_IDX_CACHED    0
#define MAIR_IDX_NONCACHED 1
#define MAIR_IDX_DEVICE    2
#define MAIR_IDX_S_ORDERED 3

#define TTL_IDX(lvl, addr) (((addr_t)(addr) >> (39 - (9 * lvl))) & (0x1ff))

#define IN_PAGE_MASK (SIZE_PAGE - 1)
#define PAGE_MASK    (~IN_PAGE_MASK)

#define ASID_NONE   0
#define ASID_SHARED 1
#define N_ASIDS     (1 << ASID_BITS)

#define PMAP_MEM_ENTRIES 64

#define CEIL_PAGE(x) ((((ptr_t)x) + SIZE_PAGE - 1) & (~(SIZE_PAGE - 1)))


typedef struct {
	addr_t start;
	addr_t end;
	int flags;
} pmap_memEntry_t;


struct {
	/* The order of fields below must be preserved */
	descr_t kernel_ttl2[SIZE_PAGE / sizeof(descr_t)];
	descr_t kernel_ttl3[SIZE_PAGE / sizeof(descr_t)];
	descr_t devices_ttl3[SIZE_PAGE / sizeof(descr_t)];
	volatile descr_t scratch_tt[SIZE_PAGE / sizeof(descr_t)]; /* Translation tables will be temporarily mapped here when needed */
	u8 stack_el0[NUM_CPUS][SIZE_INITIAL_KSTACK];
	u8 stack_el1[CEIL_PAGE(NUM_CPUS * SIZE_SP_EL1)];
	u8 heap[SIZE_PAGE];
	/* The fields below may be reordered */

	descr_t kernel_ttl1[SIZE_PAGE / sizeof(descr_t)]; /* Not used by hardware */
	/* Accesses to this struct don't need to be mutexed, because it isn't modified
	 * after initialization */
	struct {
		addr_t min;
		addr_t max;

		u64 dtb;
		u64 dtbsz;

		addr_t pkernel;
		size_t kernelsz;
		ptr_t vkernelEnd;

		pmap_memEntry_t entries[PMAP_MEM_ENTRIES];
		size_t count;
	} mem;

	u64 asidInUse[N_ASIDS / 64];
	u64 asidOverflow;
	unsigned firstFreeAsid;

	addr_t start;
	addr_t end;
	spinlock_t lock;

	size_t dev_i;
} __attribute__((aligned(SIZE_PAGE))) pmap_common;


static const char *const marksets[4] = { "BBBBBBBBBBBBBBBB", "KYCPMSHKKKKKKKKK", "AAAAAAAAAAAAAAAA", "UUUUUUUUUUUUUUUU" };


static void _pmap_asidAlloc(pmap_t *pmap)
{
	unsigned i, assigned;
	u64 free;
	if (pmap_common.firstFreeAsid == N_ASIDS) {
		assigned = ASID_SHARED;
		pmap_common.asidOverflow++;
	}
	else {
		assigned = pmap_common.firstFreeAsid;
		pmap_common.asidInUse[assigned / 64] |= 1uL << (assigned % 64);
		for (i = assigned / 64; i < (N_ASIDS / 64); i++) {
			free = ~pmap_common.asidInUse[i];
			if (free != 0) {
				pmap_common.firstFreeAsid = i * 64 + hal_cpuGetFirstBit(free);
				break;
			}
		}

		if (i == N_ASIDS / 64) {
			pmap_common.firstFreeAsid = N_ASIDS;
		}
	}

	pmap->asid = assigned;
}


static void _pmap_asidDealloc(pmap_t *pmap)
{
	if (pmap->asid == ASID_NONE) {
		return;
	}

	if (pmap->asid == ASID_SHARED) {
		pmap_common.asidOverflow--;
		pmap->asid = ASID_NONE;
		return;
	}

	if (pmap->asid < pmap_common.firstFreeAsid) {
		pmap_common.firstFreeAsid = pmap->asid;
	}

	pmap_common.asidInUse[pmap->asid / 64] &= ~(1uL << (pmap->asid % 64));
	hal_tlbInvalASID_IS(pmap->asid);
	pmap->asid = ASID_NONE;
}


static void pmap_cacheOpOnChange(descr_t oldEntry, descr_t newEntry, void *vaddr, unsigned lvl)
{
	if (lvl != 3) {
		/* TODO: would be nice to do it if we ever support large mappings */
		return;
	}

	if (((oldEntry & DESCR_AP2) == 0) && (ATTR_FROM_DESCR(oldEntry) == MAIR_IDX_CACHED)) {
		if (newEntry != 0) {
			hal_cpuFlushDataCache((ptr_t)vaddr, (ptr_t)vaddr + SIZE_PAGE);
		}
		else {
			hal_cpuCleanDataCache((ptr_t)vaddr, (ptr_t)vaddr + SIZE_PAGE);
		}
	}

	if (((newEntry & (DESCR_PXN | DESCR_UXN)) == 0) || ((oldEntry & (DESCR_PXN | DESCR_UXN)) == 0)) {
		hal_cpuICacheInval();
	}
}


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	pmap->ttl1 = vaddr;
	pmap->addr = p->addr;
	pmap->asid = ASID_NONE;

	hal_memset(pmap->ttl1, 0, SIZE_PDIR);

	hal_cpuDataMemoryBarrier();
	hal_cpuDataSyncBarrier();
	return EOK;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	spinlock_ctx_t sc;
	descr_t entry;
	const unsigned max = (((VADDR_USR_MAX - 1) >> 30) & 0x1ff);

	hal_spinlockSet(&pmap_common.lock, &sc);
	if (pmap->asid != ASID_NONE) {
		_pmap_asidDealloc(pmap);
	}

	while (*i <= max) {
		entry = pmap->ttl1[*i];
		(*i)++;
		if ((entry & DESCR_VALID) == 0) {
			hal_spinlockClear(&pmap_common.lock, &sc);
			return DESCR_PA(entry);
		}
	}

	hal_spinlockClear(&pmap_common.lock, &sc);
	return 0;
}


void _pmap_switch(pmap_t *pmap)
{
	if ((ptr_t)pmap->start >= VADDR_KERNEL) {
		/* Kernel pmap doesn't need to be switched in, also this function cannot do it. */
		return;
	}
	else if (pmap->asid == ASID_NONE) {
		_pmap_asidAlloc(pmap);
	}
	else if (hal_cpuGetTranslationBase() == pmap->addr) {
		return;
	}

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
	hal_cpuSetTranslationBase(pmap->addr, pmap->asid);
	hal_cpuInstrBarrier();

	if (pmap->asid == ASID_SHARED) {
		hal_tlbInvalASID(ASID_SHARED);
	}

	hal_cpuICacheInval();
}


void pmap_switch(pmap_t *pmap)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);
	_pmap_switch(pmap);
	hal_spinlockClear(&pmap_common.lock, &sc);
}


static void _pmap_writeTtl3(volatile descr_t *ptable, void *va, addr_t pa, int attributes, unsigned char asid)
{
	unsigned idx = TTL_IDX(3, va);
	descr_t descr, oldDescr;

	hal_cpuCleanDataCache((ptr_t)&ptable[idx], (ptr_t)&ptable[idx] + sizeof(descr_t));
	oldDescr = ptable[idx];

	if ((attributes & PGHD_PRESENT) == 0) {
		descr = 0;
	}
	else {
		descr = DESCR_PA(pa) | DESCR_VALID | DESCR_TABLE | DESCR_AF | DESCR_ISH;
		if ((ptr_t)va < VADDR_USR_MAX) {
			descr |= DESCR_nG;
		}

		if ((attributes & PGHD_EXEC) == 0) {
			descr |= DESCR_PXN | DESCR_UXN;
		}

		if ((attributes & PGHD_WRITE) == 0) {
			descr |= DESCR_AP2;
		}

		if ((attributes & PGHD_USER) != 0) {
			descr |= DESCR_AP1;
		}

		switch (attributes & (PGHD_NOT_CACHED | PGHD_DEV)) {
			/* TODO: does this make sense - NOT_CACHED and DEV flags will result in strongly-ordered memory */
			case PGHD_NOT_CACHED | PGHD_DEV:
				descr |= DESCR_ATTR(MAIR_IDX_S_ORDERED);
				break;

			case PGHD_NOT_CACHED:
				descr |= DESCR_ATTR(MAIR_IDX_NONCACHED);
				break;

			case PGHD_DEV:
				descr |= DESCR_ATTR(MAIR_IDX_DEVICE);
				break;

			default:
				descr |= DESCR_ATTR(MAIR_IDX_CACHED);
				break;
		}
	}

	if ((oldDescr & DESCR_VALID) != 0) {
		pmap_cacheOpOnChange(oldDescr, descr, va, 3);
		/* D8.16.1 Using break-before-make when updating translation table entries
		 * TODO: verify if it needs to be done in this case
		 */
		ptable[idx] = 0;
		hal_cpuDataSyncBarrier();
		hal_tlbInvalVAASID_IS((ptr_t)va, asid);
		hal_cpuDataSyncBarrier();
	}

	ptable[idx] = descr;
	hal_cpuDataSyncBarrier();
}


/* Maps one page of scratch memory as non-cached, non-shareable. Must be used under mutex. */
static void _pmap_mapScratch(addr_t pa)
{
	u64 tlbiArg = ((ptr_t)pmap_common.scratch_tt >> 12) & ((1uL << 44) - 1);
	pmap_common.kernel_ttl3[TTL_IDX(3, pmap_common.scratch_tt)] =
			DESCR_PA(pa) | DESCR_VALID | DESCR_TABLE | DESCR_AF | DESCR_ATTR(MAIR_IDX_NONCACHED) | DESCR_PXN | DESCR_UXN | DESCR_nSH;
	/* Invalidate last level only for a bit more performance */
	__asm__ volatile ("tlbi vaale1, %0" : : "r"(tlbiArg));
}


/* Functions maps page at specified address */
int _pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc)
{
	int doInit = 0;
	unsigned i, lvl;
	volatile descr_t *tt;
	descr_t entry;
	addr_t addr;
	unsigned char asid = pmap->asid;

	/* If no page table is allocated add new one */
	tt = pmap->ttl1;
	for (lvl = 1; lvl <= 2; lvl++) {
		entry = tt[TTL_IDX(lvl, vaddr)];
		if ((entry & DESCR_VALID) == 0) {
			if (alloc == NULL) {
				return -EFAULT;
			}

			addr = alloc->addr;
			tt[TTL_IDX(lvl, vaddr)] = DESCR_PA(addr) | DESCR_VALID | DESCR_TABLE;
			doInit = 1;
			alloc = NULL;
		}
		else if ((entry & DESCR_TABLE) == 0) {
			/* Already mapped as a block - not supported right now */
			return -EINVAL;
		}
		else {
			addr = DESCR_PA(entry);
		}

		_pmap_mapScratch(addr);
		tt = pmap_common.scratch_tt;
		if (doInit != 0) {
			doInit = 0;
			for (i = 0; i < SIZE_PAGE / sizeof(descr_t); i++) {
				pmap_common.scratch_tt[i] = 0;
			}
		}
	}

	_pmap_writeTtl3(tt, vaddr, pa, attr, asid);

	if ((attr & (PGHD_NOT_CACHED | PGHD_DEV)) != 0) {
		/* TODO: this was done on AArch32 - verify if it is still necessary */
		/* Invalidate cache for this pa to prevent corrupting it later when cache lines get evicted.
		 * First map it into our address space if necessary. */
		if (hal_cpuGetTranslationBase() != pmap->addr) {
			_pmap_mapScratch(pa);
			vaddr = (void *)pmap_common.scratch_tt;
		}

		hal_cpuFlushDataCache((ptr_t)vaddr, (ptr_t)vaddr + SIZE_PAGE);
	}

	return EOK;
}


int pmap_enter(pmap_t *pmap, addr_t paddr, void *vaddr, int attr, page_t *alloc)
{
	int ret;
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);
	ret = _pmap_enter(pmap, paddr, vaddr, attr, alloc);
	hal_spinlockClear(&pmap_common.lock, &sc);
	return ret;
}


void _pmap_remove(pmap_t *pmap, void *vaddr)
{
	unsigned lvl;
	volatile descr_t *tt;
	descr_t entry;
	addr_t addr;

	tt = pmap->ttl1;
	for (lvl = 1; lvl <= 3; lvl++) {
		entry = tt[TTL_IDX(lvl, vaddr)];
		if ((entry & DESCR_VALID) == 0) {
			return;
		}
		else if ((lvl == 3) || ((entry & DESCR_TABLE) == 0)) {
			break;
		}
		else {
			addr = DESCR_PA(entry);
			_pmap_mapScratch(addr);
			tt = pmap_common.scratch_tt;
		}
	}

	pmap_cacheOpOnChange(entry, 0, vaddr, lvl);

	tt[TTL_IDX(lvl, vaddr)] = 0;
	hal_cpuDataSyncBarrier();
	hal_tlbInvalVA_IS((ptr_t)vaddr);
	hal_cpuInstrBarrier();
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);
	_pmap_remove(pmap, vaddr);
	hal_spinlockClear(&pmap_common.lock, &sc);

	return EOK;
}


/* Functions returns physical address associated with specified virtual address */
addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	unsigned lvl;
	volatile descr_t *tt;
	descr_t entry;
	addr_t addr = (addr_t)vaddr;
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);
	if (((addr_t)vaddr < VADDR_USR_MAX) && (hal_cpuGetTranslationBase() != pmap->addr)) {
		tt = pmap->ttl1;
		for (lvl = 1; lvl <= 3; lvl++) {
			entry = tt[TTL_IDX(lvl, vaddr)];
			if ((entry & DESCR_VALID) == 0) {
				addr = 1;
				break;
			}
			else if ((lvl == 3) || ((entry & DESCR_TABLE) == 0)) {
				addr = DESCR_PA(entry);
				break;
			}
			else {
				addr = DESCR_PA(entry);
				_pmap_mapScratch(addr);
				tt = pmap_common.scratch_tt;
			}
		}
	}
	else {
		/* When translating from common or current address space we can just use AT instruction */
		__asm__ volatile (
				"at s1e1r, %0\n"
				"mrs %0, par_el1\n"
				: "+r"(addr) : : "memory");
	}

	hal_spinlockClear(&pmap_common.lock, &sc);
	if ((addr & 1) == 0) {
		return addr & ((1uL << 48) - (1uL << 12));
	}
	else {
		return 0;
	}
}


/* Function fills page_t structure for frame given by addr */
int pmap_getPage(page_t *page, addr_t *addr)
{
	int found;
	size_t i;
	addr_t a, next_a;
	const syspage_prog_t *prog;
	const pmap_memEntry_t *entry;

	a = *addr & ~(SIZE_PAGE - 1);
	page->flags = 0;

	/* Test address ranges */
	if (a < pmap_common.mem.min) {
		a = pmap_common.mem.min;
	}

	if (a > pmap_common.mem.max) {
		return -ENOMEM;
	}

	page->addr = a;
	found = 0;
	for (i = 0; i < pmap_common.mem.count; i++) {
		entry = &pmap_common.mem.entries[i];
		if (found == 0) {
			if ((a >= entry->start) && (a <= entry->end)) {
				page->flags = entry->flags;
				found = 1;
			}
		}

		if (found == 1) {
			next_a = a + SIZE_PAGE;
			if (next_a <= entry->end) {
				if (next_a < entry->start) {
					next_a = entry->start;
				}

				*addr = next_a;
				found = 2;
				break;
			}
		}
	}

	if (found == 0) {
		return -EINVAL;
	}
	else if (found == 1) {
		*addr = 0;
	}

	if (syspage->progs != NULL) {
		prog = syspage->progs;
		do {
			if (page->addr >= prog->start && page->addr < prog->end) {
				page->flags |= PAGE_OWNER_APP;
				return EOK;
			}
			prog = prog->next;
		} while (prog != syspage->progs);
	}

	if ((page->addr >= pmap_common.mem.pkernel) && (page->addr < (pmap_common.mem.pkernel + pmap_common.mem.kernelsz))) {
		page->flags |= PAGE_OWNER_KERNEL;

		if ((page->addr >= (ptr_t)pmap_common.kernel_ttl2) && (page->addr < ((ptr_t)pmap_common.devices_ttl3 + sizeof(pmap_common.devices_ttl3)))) {
			page->flags |= PAGE_KERNEL_PTABLE;
		}

		if ((page->addr >= (ptr_t)pmap_common.stack_el0) && (page->addr < ((ptr_t)pmap_common.stack_el1 + sizeof(pmap_common.stack_el1)))) {
			page->flags |= PAGE_KERNEL_STACK;
		}
	}
	else if ((page->addr >= pmap_common.mem.dtb) && (page->addr < (pmap_common.mem.dtb + pmap_common.mem.dtbsz))) {
		page->flags |= PAGE_OWNER_BOOT;
	}
	else {
		page->flags |= PAGE_FREE;
	}

	return EOK;
}


/* Function allocates page tables for kernel space */
int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp)
{
	ptr_t vaddr;

	vaddr = CEIL_PAGE(*start);
	if (vaddr >= (ptr_t)end) {
		return EOK;
	}

	if (vaddr < VADDR_KERNEL) {
		vaddr = VADDR_KERNEL;
	}

	for (; vaddr < (ptr_t)end; vaddr += (SIZE_PAGE << 9)) {
		if (pmap_enter(pmap, 0, (void *)vaddr, ~PGHD_PRESENT, NULL) < 0) {
			if (pmap_enter(pmap, 0, (void *)vaddr, ~PGHD_PRESENT, dp) < 0) {
				return -ENOMEM;
			}
			dp = NULL;
		}
		*start = (void *)vaddr;
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


/* Translates virtual address to physical address for initial mappings only */
static addr_t _pmap_kernelVAtoPA(void *va)
{
	return (ptr_t)va - VADDR_KERNEL + pmap_common.mem.pkernel;
}


/* Function initializes low-level page mapping interface */
void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	u64 i;
	/* Zero out initial mappings, which are not needed anymore (just in case someone tries to use them) */
	for (i = 0; i < SIZE_PAGE / sizeof(descr_t); i++) {
		pmap_common.scratch_tt[i] = 0;
	}

	pmap_common.firstFreeAsid = ASID_SHARED + 1;
	pmap_common.asidOverflow = 0;
	hal_memset(pmap_common.asidInUse, 0, sizeof(pmap_common.asidInUse));
	pmap_common.asidInUse[(ASID_SHARED / 64)] |= 1uL << (ASID_SHARED % 64);
	pmap_common.asidInUse[(ASID_NONE / 64)] |= 1uL << (ASID_NONE % 64);

	pmap->asid = ASID_NONE;
	hal_spinlockCreate(&pmap_common.lock, "pmap_common.lock");

	/* Initialize kernel page table */
	pmap->ttl1 = pmap_common.kernel_ttl1;
	pmap->addr = _pmap_kernelVAtoPA(pmap_common.kernel_ttl1);

	/* Create kernel TTL1 - it is only used by software, but still needs to be initialized */
	hal_memset(pmap_common.kernel_ttl1, 0, sizeof(pmap_common.kernel_ttl1));
	pmap_common.kernel_ttl1[TTL_IDX(1, VADDR_KERNEL)] = DESCR_PA(_pmap_kernelVAtoPA(pmap_common.kernel_ttl2)) | DESCR_TABLE | DESCR_VALID;

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	/* Initialize kernel heap start address */
	(*vstart) = (void *)pmap_common.mem.vkernelEnd;
	(*vend) = (*vstart) + SIZE_PAGE;

	pmap_common.start = _pmap_kernelVAtoPA(pmap_common.heap);
	pmap_common.end = pmap_common.start + SIZE_PAGE;

	/* Create initial heap */
	pmap_enter(pmap, pmap_common.start, (*vstart), PGHD_WRITE | PGHD_READ | PGHD_PRESENT, NULL);

	hal_tlbInvalAll_IS();
}


void _pmap_preinit(addr_t dtbStart, addr_t dtbEnd, size_t memLimit)
{
	dtb_memBank_t *banks;
	size_t nBanks, sz;
	addr_t end;
	descr_t attrs = DESCR_VALID | DESCR_TABLE | DESCR_AF | DESCR_ISH | DESCR_PXN | DESCR_UXN | DESCR_ATTR(MAIR_IDX_CACHED) | DESCR_AP2;
	u64 i;

	pmap_common.dev_i = 0;

	pmap_common.mem.dtb = dtbStart & ~(SIZE_PAGE - 1);
	pmap_common.mem.dtbsz = CEIL_PAGE(dtbEnd) - pmap_common.mem.dtb;
	for (i = 0; i < pmap_common.mem.dtbsz; i += SIZE_PAGE) {
		pmap_common.devices_ttl3[TTL_IDX(3, VADDR_DTB + i)] = DESCR_PA(dtbStart + i) | attrs;
	}

	_dtb_init(dtbStart);

	pmap_common.mem.pkernel = syspage->pkernel;
	pmap_common.mem.kernelsz = CEIL_PAGE(&_end) - (addr_t)VADDR_KERNEL;
	pmap_common.mem.vkernelEnd = CEIL_PAGE(&_end);
	pmap_common.mem.min = banks[0].start;
	pmap_common.mem.max = banks[0].end;

	dtb_getMemory(&banks, &nBanks);
	pmap_common.mem.count = 0;
	for (i = 0; i < nBanks; i++) {
		end = banks[i].end;
		sz = end - banks[i].start + 1;
		if (memLimit == 0) {
			break;
		}
		else if (memLimit < sz) {
			end = banks[i].start + memLimit - 1;
			memLimit = 0;
		}
		else {
			memLimit -= sz;
		}

		if (pmap_common.mem.min > banks[i].start) {
			pmap_common.mem.min = banks[i].start;
		}

		if (pmap_common.mem.max < end) {
			pmap_common.mem.max = end;
		}

		if ((i > 0) && (banks[i].start == (pmap_common.mem.entries[pmap_common.mem.count - 1].end + 1))) {
			pmap_common.mem.entries[pmap_common.mem.count - 1].end = end;
		}
		else {
			pmap_common.mem.entries[pmap_common.mem.count].start = banks[i].start;
			pmap_common.mem.entries[pmap_common.mem.count].end = end;
			pmap_common.mem.entries[pmap_common.mem.count].flags = 0;
			pmap_common.mem.count++;
		}
	}

	/* Set code to read-only, everthing else XN and remove mappings past the end */
	for (i = 0; i < TTL_IDX(3, CEIL_PAGE(&_etext)); i++) {
		pmap_common.kernel_ttl3[i] |= DESCR_AP2;
	}

	for (; i < TTL_IDX(3, pmap_common.mem.vkernelEnd); i++) {
		pmap_common.kernel_ttl3[i] |= DESCR_PXN | DESCR_UXN;
	}

	for (; i < (SIZE_PAGE / sizeof(descr_t)); i++) {
		pmap_common.kernel_ttl3[i] = 0;
	}

	hal_tlbInvalAll_IS();
}


void *_pmap_halMapDevice(addr_t paddr, size_t pageOffs, size_t size)
{
	descr_t attrs = DESCR_VALID | DESCR_TABLE | DESCR_AF | DESCR_ISH | DESCR_PXN | DESCR_UXN | DESCR_ATTR(MAIR_IDX_DEVICE);
	ptr_t va_start = ((VADDR_MAX - (SIZE_PAGE << 9)) + 1) + (pmap_common.dev_i * SIZE_PAGE);
	ptr_t va = va_start;
	addr_t end;
	if (pmap_common.dev_i >= TTL_IDX(3, VADDR_DTB)) {
		return 0;
	}

	for (end = paddr + size; paddr < end; paddr += SIZE_PAGE, va += SIZE_PAGE) {
		pmap_common.devices_ttl3[TTL_IDX(3, va)] = DESCR_PA(paddr) | attrs;
		pmap_common.dev_i++;
	}

	return (void *)va_start + pageOffs;
}