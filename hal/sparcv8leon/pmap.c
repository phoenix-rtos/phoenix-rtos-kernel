/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem
 *
 * Copyright 2022, 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch/tlb.h>

#include "hal/cpu.h"
#include "hal/pmap.h"
#include "hal/hal.h"
#include "hal/string.h"
#include "hal/spinlock.h"
#include "hal/sparcv8leon/sparcv8leon.h"
#include "hal/tlb/tlb.h"
#include "gaisler/l2cache.h"

#include "include/errno.h"
#include "include/mman.h"

#include "halsyspage.h"


#define MAX_CONTEXTS    256
#define CONTEXT_INVALID 0xffffffffu
#define CONTEXT_SHARED  255

#define PDIR1_IDX(vaddr) ((ptr_t)(vaddr) >> 24)
#define PDIR2_IDX(vaddr) (((ptr_t)(vaddr) >> 18) & 0x3f)
#define PDIR3_IDX(vaddr) (((ptr_t)(vaddr) >> 12) & 0x3f)

#define UNCACHED 0
#define CACHED   1

/* Page Table Descriptor */
#define PTD(paddr)       ((((u32)(paddr) >> 6) << 2) | PAGE_DESCR)
#define PTD_TO_ADDR(ptd) (((u32)(ptd) >> 2) << 6)
/* Page Table Entry */
#define PTE(paddr, c, acc, type) ((((u32)(paddr) >> 12) << 8) | ((c & 0x1) << 7) | ((acc & 0x7) << 2) | (type & 0x3))
#define PTE_TO_ADDR(pte)         (((u32)(pte) >> 8) << 12)

#define CEIL_PAGE(x) ((((addr_t)x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


/* Linker symbols */
extern unsigned int _start;
extern unsigned int _end;
extern unsigned int _etext;
extern unsigned int __bss_start;


#define PMAP_MEM_ENTRIES 64


typedef struct {
	addr_t start;
	size_t pageCount;
	int flags;
} pmap_memEntry_t;


struct {
	/* Order of these field must be strictly preserved */
	u32 ctxTable[256];
	u32 pdir1[256];
	u32 pdir2[64];
	u32 pdir3[64][64] __attribute__((aligned(SIZE_PAGE)));

	u8 heap[SIZE_PAGE] __attribute__((aligned(SIZE_PAGE)));
	u8 stack[NUM_CPUS][SIZE_KSTACK] __attribute__((aligned(SIZE_PAGE)));
	u32 ctxMap[MAX_CONTEXTS / 32]; /* Bitmap of context numbers, 0 = taken, 1 = free */
	u32 numCtxFree;

	addr_t minAddr;
	addr_t maxAddr;

	u32 start;
	u32 end;
	spinlock_t lock;

	addr_t kernel;
	size_t kernelsz;
	ptr_t vkernelEnd;

	struct {
		pmap_memEntry_t entries[PMAP_MEM_ENTRIES];
		size_t count;
	} memMap;
	addr_t pageIterator;
} pmap_common __attribute__((aligned(SIZE_PAGE)));


static u32 pmap_attrToAcc(u32 attr)
{
	u32 acc;

	attr &= 0xf; /* Mask out cache, dev & present bits */

	if ((attr & PGHD_USER) != 0) {
		/* Mask out user bit */
		switch (attr & 0x7) {
			case (PGHD_READ):
				acc = PERM_USER_RO;
				break;

			case (PGHD_WRITE):
			case (PGHD_READ | PGHD_WRITE):
				acc = PERM_USER_RW;
				break;

			case (PGHD_READ | PGHD_EXEC):
				acc = PERM_USER_RX;
				break;

			case (PGHD_READ | PGHD_WRITE | PGHD_EXEC):
				acc = PERM_USER_RWX;
				break;

			case (PGHD_EXEC):
				acc = PERM_USER_XO;
				break;

			default:
				acc = PERM_USER_RO;
				break;
		}
	}
	else {
		switch (attr) {
			case (PGHD_READ):
			case (PGHD_WRITE):
			case (PGHD_READ | PGHD_WRITE):
				acc = PERM_SUPER_RW;
				break;

			case (PGHD_READ | PGHD_EXEC):
				acc = PERM_SUPER_RX;
				break;

			case (PGHD_EXEC):
			case (PGHD_READ | PGHD_WRITE | PGHD_EXEC):
				acc = PERM_SUPER_RWX;
				break;

			default:
				acc = PERM_SUPER_RW;
				break;
		}
	}

	return acc;
}


static u32 _pmap_contextAlloc(void)
{
	u8 ctxId;

	if (pmap_common.numCtxFree != 0) {
		for (size_t i = 0; i < (MAX_CONTEXTS / 32); i++) {
			if (pmap_common.ctxMap[i] != 0x0) {
				ctxId = hal_cpuGetFirstBit(pmap_common.ctxMap[i]);
				pmap_common.ctxMap[i] &= ~(1 << ctxId);
				pmap_common.numCtxFree--;
				return (i * 32) + ctxId;
			}
		}
	}

	return CONTEXT_SHARED;
}


static void _pmap_contextDealloc(pmap_t *pmap)
{
	u32 ctxId = pmap->context;
	if (ctxId != CONTEXT_SHARED) {
		pmap_common.ctxMap[ctxId / 32] |= (1 << (ctxId % 32));
		pmap_common.numCtxFree++;
	}
	pmap->context = CONTEXT_INVALID;
}


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	pmap->pdir1 = vaddr;
	pmap->context = CONTEXT_INVALID;
	hal_memset(pmap->pdir1, 0, 256 * sizeof(u32));
	hal_memcpy(
			&pmap->pdir1[PDIR1_IDX(VADDR_KERNEL)],
			&kpmap->pdir1[PDIR1_IDX(VADDR_KERNEL)],
			(VADDR_MAX - VADDR_KERNEL + 1) >> 24);

	return 0;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	const u32 idx1 = PDIR1_IDX(VADDR_USR_MAX);
	spinlock_ctx_t sc;
	addr_t pdir2, pdir3;
	u32 j;

	if (pmap->context != CONTEXT_INVALID) {
		hal_spinlockSet(&pmap_common.lock, &sc);
		pmap_common.ctxTable[pmap->context] = NULL;
		_pmap_contextDealloc(pmap);
		hal_spinlockClear(&pmap_common.lock, &sc);
	}

	while (*i < idx1) {
		pdir2 = PTD_TO_ADDR(pmap->pdir1[*i]);
		if (pdir2 != NULL) {
			for (j = 0; j < 64; j++) {
				pdir3 = PTD_TO_ADDR(hal_cpuLoadPaddr(&((u32 *)pdir2)[j]));
				if (pdir3 != NULL) {
					hal_cpuStorePaddr(&((u32 *)pdir2)[j], 0);
					hal_cpuflushDCacheL1();

					return pdir3;
				}
			}
			(*i)++;

			return pdir2;
		}
		(*i)++;
	}

	return 0;
}


static addr_t _pmap_resolve(pmap_t *pmap, void *vaddr)
{
	u32 idx1 = PDIR1_IDX(vaddr), idx2 = PDIR2_IDX(vaddr), idx3 = PDIR3_IDX(vaddr);
	addr_t addr;

	addr = PTD_TO_ADDR(pmap->pdir1[idx1]);
	if (addr == 0) {
		return 0;
	}

	addr = PTD_TO_ADDR(hal_cpuLoadPaddr(&((u32 *)addr)[idx2]));
	if (addr == 0) {
		return 0;
	}

	addr = PTE_TO_ADDR(hal_cpuLoadPaddr(&((u32 *)addr)[idx3]));

	return addr;
}


/* Functions returns physical address associated with specified virtual address */
addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	addr_t addr;
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);

	addr = _pmap_resolve(pmap, vaddr);

	hal_spinlockClear(&pmap_common.lock, &sc);

	return addr;
}

void pmap_switch(pmap_t *pmap)
{
	spinlock_ctx_t sc;
	addr_t paddr;

	hal_spinlockSet(&pmap_common.lock, &sc);
	if (hal_srmmuGetContext() == CONTEXT_SHARED) {
		hal_cpuflushICacheL1();
		hal_cpuflushDCacheL1();
#ifdef LEON_HAS_L2CACHE
		l2c_flushRange(l2c_flush_inv_all, 0, 0);
#endif
	}

	if ((pmap->context == CONTEXT_INVALID) || ((pmap->context == CONTEXT_SHARED) && (pmap_common.numCtxFree != 0))) {
		pmap->context = _pmap_contextAlloc();
		paddr = PTD(_pmap_resolve(pmap, pmap->pdir1) + ((u32)pmap->pdir1 & 0xfff));
		pmap_common.ctxTable[pmap->context] = paddr;
	}

	hal_srmmuSetContext(pmap->context);

	if (pmap->context == CONTEXT_SHARED) {
		paddr = PTD(_pmap_resolve(pmap, pmap->pdir1) + ((u32)pmap->pdir1 & 0xfff));
		pmap_common.ctxTable[CONTEXT_SHARED] = paddr;
		hal_srmmuFlushTLB(0, TLB_FLUSH_CTX);
	}

	hal_spinlockClear(&pmap_common.lock, &sc);
}


static int _pmap_map(u32 *pdir1, addr_t pa, void *vaddr, int attr, page_t *alloc)
{
	addr_t addr, pdir2;
	u8 idx1 = PDIR1_IDX(vaddr), idx2 = PDIR2_IDX(vaddr), idx3 = PDIR3_IDX(vaddr);
	u32 acc = pmap_attrToAcc(attr), entry;

	addr = PTD_TO_ADDR(pdir1[idx1]);

	if (addr == 0) {
		/* Allocate PDIR2 */
		if (alloc == NULL) {
			return -ENOMEM;
		}

		for (size_t i = 0; i < (SIZE_PAGE / sizeof(u32)); i++) {
			hal_cpuStorePaddr((u32 *)alloc->addr + i, 0);
		}
		hal_cpuflushDCacheL1();

		pdir1[idx1] = PTD(alloc->addr);

		addr = PTD_TO_ADDR(pdir1[idx1]);

		alloc = NULL;
	}

	/* addr points to 2nd level table */
	pdir2 = addr;

	/* Check if PDIR3 is allocated */
	addr = PTD_TO_ADDR(hal_cpuLoadPaddr(&((u32 *)addr)[idx2]));

	if (addr == 0) {
		/* Allocate PDIR3 */
		if (alloc == NULL) {
			return -EFAULT;
		}

		for (size_t i = 0; i < (SIZE_PAGE / sizeof(u32)); i++) {
			hal_cpuStorePaddr((u32 *)alloc->addr + i, 0);
		}

		hal_cpuStorePaddr(&((u32 *)pdir2)[idx2], PTD(alloc->addr));
		hal_cpuflushDCacheL1();

		addr = PTD_TO_ADDR(hal_cpuLoadPaddr(&((u32 *)pdir2)[idx2]));

		alloc = NULL;
	}

#ifdef LEON_HAS_L2CACHE
	if ((attr & (PGHD_NOT_CACHED | PGHD_DEV)) == 0) {
		l2c_flushRange(l2c_flush_inv_line, (ptr_t)vaddr, SIZE_PAGE);
	}
#endif

	entry = PTE(pa, ((attr & (PGHD_NOT_CACHED | PGHD_DEV)) != 0) ? UNCACHED : CACHED, acc, ((attr & PGHD_PRESENT) != 0) ? PAGE_ENTRY : 0);

	hal_cpuStorePaddr(&((u32 *)addr)[idx3], entry);
	hal_cpuflushDCacheL1();

	if ((attr & PGHD_EXEC) != 0) {
		hal_cpuflushICacheL1();
	}


	return EOK;
}


static int _pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc, int tlbInval)
{
	int ret = _pmap_map(pmap->pdir1, pa, vaddr, attr, alloc);
	if (ret < 0) {
		return ret;
	}

	if (tlbInval != 0) {
		hal_tlbInvalidateEntry(pmap, vaddr, 1);
	}
	else {
		hal_tlbInvalidateLocalEntry(pmap, vaddr);
	}

	return EOK;
}


/* Functions maps page at specified address */
int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc)
{
	int ret;
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);
	ret = _pmap_enter(pmap, pa, vaddr, attr, alloc, 1);
	if (ret == EOK) {
		hal_tlbCommit(&pmap_common.lock, &sc);
	}
	else {
		hal_spinlockClear(&pmap_common.lock, &sc);
	}
	return ret;
}


int pmap_remove(pmap_t *pmap, void *vstart, void *vend)
{
	size_t idx1, idx2, idx3;
	addr_t addr = 0, descr;
	spinlock_ctx_t sc;
	ptr_t vaddr;
	int foundidx3 = 0;

	hal_spinlockSet(&pmap_common.lock, &sc);

	for (vaddr = (ptr_t)vstart; vaddr < (ptr_t)vend; vaddr += SIZE_PAGE) {
		idx1 = PDIR1_IDX(vaddr);
		idx2 = PDIR2_IDX(vaddr);
		idx3 = PDIR3_IDX(vaddr);

		if ((foundidx3 == 0) || (idx3 == 0)) {
			foundidx3 = 0; /* Set when idx3 = 0 */
			descr = pmap->pdir1[idx1];

			if ((descr & 0x3) == PAGE_INVALID) {
				continue;
			}

			addr = PTD_TO_ADDR(descr);
			descr = hal_cpuLoadPaddr(&((u32 *)addr)[idx2]);

			if ((descr & 0x3) == PAGE_INVALID) {
				continue;
			}
			addr = PTD_TO_ADDR(descr);
			foundidx3 = 1;
		}

		hal_cpuflushDCacheL1();
#ifdef LEON_HAS_L2CACHE
		l2c_flushRange(l2c_flush_inv_line, (ptr_t)vaddr, SIZE_PAGE);
#endif
		hal_cpuStorePaddr(&((u32 *)addr)[idx3], 0);

#ifdef __CPU_GR712RC /* Errata */
		hal_cpuflushDCacheL1();
#endif
	}

	hal_tlbInvalidateEntry(pmap, vstart, CEIL_PAGE((ptr_t)vend - (ptr_t)vstart) / SIZE_PAGE);

	hal_tlbCommit(&pmap_common.lock, &sc);

	return EOK;
}


int pmap_getPage(page_t *page, addr_t *addr)
{
	size_t i;
	addr_t a, min, max;
	spinlock_ctx_t sc;
	const syspage_prog_t *prog;
	const pmap_memEntry_t *entry;

	a = (*addr) & ~(SIZE_PAGE - 1);
	page->flags = 0;

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
	page->flags = 0;
	(*addr) = a + SIZE_PAGE;

	for (i = 0; i < pmap_common.memMap.count; i++) {
		entry = &pmap_common.memMap.entries[i];
		if ((a >= entry->start) && (a < entry->start + entry->pageCount * SIZE_PAGE)) {
			page->flags = entry->flags;
			return EOK;
		}
	}

	prog = syspage->progs;
	if (prog != NULL) {
		do {
			if ((page->addr >= prog->start) && (page->addr < prog->end)) {
				page->flags = PAGE_OWNER_APP;
				return EOK;
			}
			prog = prog->next;
		} while (prog != syspage->progs);
	}


	if ((page->addr >= pmap_common.kernel) && (page->addr < pmap_common.kernel + pmap_common.kernelsz)) {
		page->flags = PAGE_OWNER_KERNEL;
		if ((page->addr >= (ptr_t)pmap_common.ctxTable) && (page->addr < (ptr_t)pmap_common.heap)) {
			page->flags |= PAGE_KERNEL_PTABLE;
		}

		if ((page->addr >= (ptr_t)pmap_common.stack) && (page->addr < ((ptr_t)pmap_common.stack + sizeof(pmap_common.stack)))) {
			page->flags |= PAGE_KERNEL_STACK;
		}

		if ((page->addr >= (ptr_t)pmap_common.heap) && (page->addr < ((ptr_t)pmap_common.heap + sizeof(pmap_common.heap)))) {
			page->flags |= PAGE_KERNEL_HEAP;
		}
	}
	else {
		page->flags |= PAGE_FREE;
	}

	return EOK;
}


char pmap_marker(page_t *p)
{
	static const char *const marksets[4] = { "BBBBBBBBBBBBBBBB", "KYCPMSHKKKKKKKKK", "AAAAAAAAAAAAAAAA", "UUUUUUUUUUUUUUUU" };

	if ((p->flags & PAGE_FREE) != 0) {
		return '.';
	}

	return marksets[(p->flags >> 1) & 3][(p->flags >> 4) & 0xf];
}


int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp)
{
	void *vaddr = (void *)((u32)(*start + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));

	if (vaddr >= end) {
		return EOK;
	}

	if (vaddr < (void *)VADDR_KERNEL) {
		vaddr = (void *)VADDR_KERNEL;
	}

	for (; vaddr < end; vaddr += (SIZE_PAGE << 10)) {
		if (_pmap_enter(pmap, 0, vaddr, ~PGHD_PRESENT, NULL, 0) < 0) {
			if (_pmap_enter(pmap, 0, vaddr, ~PGHD_PRESENT, dp, 0) < 0) {
				return -ENOMEM;
			}
			dp = NULL;
		}
		(*start) = vaddr;
	}
	hal_srmmuFlushTLB(NULL, ASI_FLUSH_ALL);

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = end;

	return EOK;
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


static int _pmap_addMemEntry(addr_t start, size_t length, int flags)
{
	addr_t end;
	size_t pageCount;

	if (pmap_common.memMap.count >= PMAP_MEM_ENTRIES) {
		return -ENOMEM;
	}

	end = CEIL_PAGE(start + length);
	pageCount = (end - start) / SIZE_PAGE;

	pmap_common.memMap.entries[pmap_common.memMap.count].start = start;
	pmap_common.memMap.entries[pmap_common.memMap.count].pageCount = pageCount;
	pmap_common.memMap.entries[pmap_common.memMap.count].flags = flags;

	pmap_common.memMap.count++;
	return EOK;
}


static int _pmap_findFreePage(page_t *page)
{
	int ret = -ENOMEM;

	while (pmap_common.pageIterator < pmap_common.maxAddr) {
		ret = pmap_getPage(page, &pmap_common.pageIterator);
		if (((page->flags & PAGE_FREE) != 0) || (ret != EOK)) {
			break;
		}
	}

	return ret;
}


static void *_pmap_halMapInternal(addr_t paddr, void *va, size_t size, int attr, int remoteFlush)
{
	void *ret;
	void **pva;
	addr_t end;
	page_t *alloc = NULL;
	page_t page;

	if ((hal_started() != 0) && (va == NULL)) {
		return NULL;
	}

	paddr &= ~(SIZE_PAGE - 1);
	end = CEIL_PAGE(paddr + size);

	/* Handle overflow, but allow mapping to the end of the physical address space (end = 0) */
	if ((end != 0) && (end < paddr)) {
		return NULL;
	}

	if (va == NULL) {
		pva = (void **)&pmap_common.vkernelEnd;
		va = *pva;
	}
	else {
		va = (void *)((ptr_t)va & ~(SIZE_PAGE - 1));
		pva = &va;
	}

	ret = va;

	while (paddr != end) {
		while (_pmap_map(pmap_common.pdir1, paddr, *pva, attr, alloc) < 0) {
			if (_pmap_findFreePage(&page) < 0) {
				if (remoteFlush != 0) {
					hal_tlbInvalidateEntry(NULL, va, CEIL_PAGE((ptr_t)*pva - (ptr_t)va) / SIZE_PAGE);
				}
				return NULL;
			}
			if (_pmap_addMemEntry(page.addr, SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE) != EOK) {
				if (remoteFlush != 0) {
					hal_tlbInvalidateEntry(NULL, va, CEIL_PAGE((ptr_t)*pva - (ptr_t)va) / SIZE_PAGE);
				}
				return NULL;
			}
			alloc = &page;
		}

		if (remoteFlush == 0) {
			hal_tlbInvalidateLocalEntry(NULL, *pva);
		}
		alloc = NULL;
		*pva += SIZE_PAGE;
		paddr += SIZE_PAGE;
	}

	if (remoteFlush != 0) {
		hal_tlbInvalidateEntry(NULL, va, CEIL_PAGE(size) / SIZE_PAGE);
	}

	return ret;
}


void *_pmap_halMap(addr_t paddr, void *va, size_t size, int attr)
{
	return _pmap_halMapInternal(paddr, va, size, attr, 0);
}


void *pmap_halMap(addr_t paddr, void *va, size_t size, int attr)
{
	void *ret;
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);
	ret = _pmap_halMapInternal(paddr, va, size, attr, 1);
	hal_tlbCommit(&pmap_common.lock, &sc);

	return ret;
}


void *_pmap_halMapDevice(addr_t paddr, size_t pageOffs, size_t size)
{
	void *ret;

	ret = _pmap_halMap(paddr, NULL, size, PGHD_WRITE | PGHD_READ | PGHD_DEV | PGHD_PRESENT);

	return (void *)((ptr_t)ret + pageOffs);
}


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	ptr_t i;
	addr_t addr;

	/* Allocate context for kernel */
	pmap->context = _pmap_contextAlloc();

	/* Initialize kernel page table */
	pmap->pdir1 = pmap_common.pdir1;
	pmap->addr = (addr_t)pmap->pdir1 - VADDR_KERNEL + pmap_common.minAddr;

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	/* Initialize kernel heap start address */
	(*vstart) = (void *)pmap_common.vkernelEnd;
	(*vend) = (*vstart) + SIZE_PAGE;

	pmap_common.start = (u32)pmap_common.heap - VADDR_KERNEL + pmap_common.kernel;
	pmap_common.end = pmap_common.start + SIZE_PAGE;

	/* Remove initial kernel mapping */
	pmap->pdir1[PDIR1_IDX(pmap_common.minAddr)] = 0;

	/* Create initial heap */
	_pmap_enter(pmap, (addr_t)pmap_common.start, (*vstart), PGHD_WRITE | PGHD_READ | PGHD_PRESENT, NULL, 0);

	/* Map kernel text & rodata as RX */
	for (i = VADDR_KERNEL; i < CEIL_PAGE((ptr_t)(&_etext)); i += SIZE_PAGE) {
		addr = pmap_common.kernel + (i - VADDR_KERNEL);
		_pmap_enter(pmap, addr, (void *)i, PGHD_READ | PGHD_EXEC | PGHD_PRESENT, NULL, 0);
	}

	/* Map kernel bss as RW */
	for (i = CEIL_PAGE((ptr_t)(&__bss_start)); i < CEIL_PAGE((ptr_t)(&_end)); i += SIZE_PAGE) {
		addr = pmap_common.kernel + (i - VADDR_KERNEL);
		_pmap_enter(pmap, addr, (void *)i, PGHD_WRITE | PGHD_READ | PGHD_PRESENT, NULL, 0);
	}
	hal_srmmuFlushTLB(0, TLB_FLUSH_ALL);
}


void _pmap_halInit(void)
{
	hal_memset(pmap_common.ctxMap, 0xff, sizeof(pmap_common.ctxMap));
	/* Context 255 is reserved as shared */
	pmap_common.ctxMap[7] &= ~(1 << 31);
	pmap_common.numCtxFree = MAX_CONTEXTS - 1;

	hal_spinlockCreate(&pmap_common.lock, "pmap_common.lock");

	pmap_common.minAddr = ADDR_RAM;
	pmap_common.maxAddr = ADDR_RAM + SIZE_RAM;

	pmap_common.pageIterator = pmap_common.minAddr;
	pmap_common.memMap.count = 0;

	pmap_common.kernel = syspage->pkernel;
	pmap_common.kernelsz = CEIL_PAGE(&_end) - VADDR_KERNEL;
	pmap_common.vkernelEnd = CEIL_PAGE(&_end);
}
