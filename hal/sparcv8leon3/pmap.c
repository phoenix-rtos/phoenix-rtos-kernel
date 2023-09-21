/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch/cpu.h>

#include "hal/cpu.h"
#include "hal/pmap.h"
#include "hal/string.h"
#include "hal/spinlock.h"
#include "hal/sparcv8leon3/sparcv8leon3.h"

#include "include/errno.h"
#include "include/mman.h"

#include "halsyspage.h"


#define MAX_CONTEXTS    256
#define CONTEXT_INVALID 0xffffffffu
#define CONTEXT_SHARED  255

#define PDIR1_IDX(vaddr) ((u32)(vaddr) >> 24)
#define PDIR2_IDX(vaddr) (((u32)(vaddr) >> 18) & 0x3f)
#define PDIR3_IDX(vaddr) (((u32)(vaddr) >> 12) & 0x3f)

#define UNCACHED 0
#define CACHED   1

/* Page Table Descriptor */
#define PTD(paddr)       ((((u32)(paddr) >> 6) << 2) | PAGE_DESCR)
#define PTD_TO_ADDR(ptd) (((u32)(ptd) >> 2) << 6)
/* Page Table Entry */
#define PTE(paddr, c, acc, type) ((((u32)(paddr) >> 12) << 8) | ((c & 0x1) << 7) | ((acc & 0x7) << 2) | (type & 0x3))
#define PTE_TO_ADDR(pte)         (((u32)(pte) >> 8) << 12)

#define CEIL_PAGE(x) (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


/* Linker symbols */
extern unsigned int _end;
extern unsigned int _etext;
extern unsigned int __bss_start;


struct {
	/* Order of these field must be strictly preserved */
	u32 ctxTable[256];
	u32 pdir1[256];
	u32 pdir2[64];
	u32 pdir3[64][64] __attribute__((aligned(SIZE_PAGE)));

	u8 heap[SIZE_PAGE] __attribute__((aligned(SIZE_PAGE)));
	u8 stack[SIZE_KSTACK] __attribute__((aligned(8)));
	u32 ctxMap[MAX_CONTEXTS / 32]; /* Bitmap of context numbers, 0 = taken, 1 = free */
	u32 numCtxFree;
	addr_t minAddr;
	addr_t maxAddr;
	u32 start;
	u32 end;
	spinlock_t lock;
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


static void _pmap_flushTLB(u32 context, void *vaddr)
{
	if (hal_srmmuGetContext() == context) {
		if ((ptr_t)vaddr < VADDR_USR_MAX) {
			hal_srmmuFlushTLB(vaddr, TLB_FLUSH_L3);
		}
		else {
			hal_srmmuFlushTLB(vaddr, TLB_FLUSH_CTX);
		}
	}
	else {
		hal_srmmuFlushTLB(vaddr, TLB_FLUSH_ALL);
	}
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
					hal_cpuflushDCache();

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
	if ((pmap->context == CONTEXT_INVALID) || ((pmap->context == CONTEXT_SHARED) && (pmap_common.numCtxFree != 0))) {
		pmap->context = _pmap_contextAlloc();
		paddr = PTD(_pmap_resolve(pmap, pmap->pdir1) + ((u32)pmap->pdir1 & 0xfff));
		pmap_common.ctxTable[pmap->context] = paddr;
	}

	hal_srmmuSetContext(pmap->context);
	hal_cpuflushICache();
	hal_cpuflushDCache();

	if (pmap->context == CONTEXT_SHARED) {
		paddr = PTD(_pmap_resolve(pmap, pmap->pdir1) + ((u32)pmap->pdir1 & 0xfff));
		pmap_common.ctxTable[CONTEXT_SHARED] = paddr;
		hal_srmmuFlushTLB(0, TLB_FLUSH_CTX);
	}

	hal_spinlockClear(&pmap_common.lock, &sc);

	return;
}


/* Functions maps page at specified address */
int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, int attr, page_t *alloc)
{
	u8 idx1 = PDIR1_IDX(vaddr), idx2 = PDIR2_IDX(vaddr), idx3 = PDIR3_IDX(vaddr);
	u8 newEntry = 0;
	addr_t addr, pdir2;
	spinlock_ctx_t sc;
	u32 acc = pmap_attrToAcc(attr), entry;

	hal_spinlockSet(&pmap_common.lock, &sc);

	addr = PTD_TO_ADDR(pmap->pdir1[idx1]);

	if (addr == 0) {
		/* Allocate PDIR2 */
		if (alloc == NULL) {
			hal_srmmuFlushTLB(0, TLB_FLUSH_ALL);
			hal_spinlockClear(&pmap_common.lock, &sc);
			return -EFAULT;
		}

		for (size_t i = 0; i < (SIZE_PAGE / sizeof(u32)); i++) {
			hal_cpuStorePaddr((u32 *)alloc->addr + i, 0);
		}
		hal_cpuflushDCache();

		pmap->pdir1[idx1] = PTD(alloc->addr);

		addr = PTD_TO_ADDR(pmap->pdir1[idx1]);

		alloc = NULL;
	}

	/* addr points to 2nd level table */
	pdir2 = addr;

	/* Check if PDIR3 is allocated */
	addr = PTD_TO_ADDR(hal_cpuLoadPaddr(&((u32 *)addr)[idx2]));

	if (addr == 0) {
		/* Allocate PDIR3 */
		if (alloc == NULL) {
			hal_srmmuFlushTLB(0, TLB_FLUSH_ALL);
			hal_spinlockClear(&pmap_common.lock, &sc);
			return -EFAULT;
		}

		for (size_t i = 0; i < (SIZE_PAGE / sizeof(u32)); i++) {
			hal_cpuStorePaddr((u32 *)alloc->addr + i, 0);
		}
		hal_cpuflushDCache();

		hal_cpuStorePaddr(&((u32 *)pdir2)[idx2], PTD(alloc->addr));
		hal_cpuflushDCache();

		addr = PTD_TO_ADDR(hal_cpuLoadPaddr(&((u32 *)pdir2)[idx2]));

		alloc = NULL;
	}

	entry = PTE(pa, ((attr & (PGHD_NOT_CACHED | PGHD_DEV)) != 0) ? UNCACHED : CACHED, acc, ((attr & PGHD_PRESENT) != 0) ? PAGE_ENTRY : 0);
	newEntry = ((hal_cpuLoadPaddr(&((u32 *)addr)[idx3]) & 0x3) == PAGE_INVALID) ? 1 : 0;

	hal_cpuStorePaddr(&((u32 *)addr)[idx3], entry);
	hal_cpuflushDCache();

	if (newEntry == 0) {
		/* Flush TLB only if entry existed earlier */
		_pmap_flushTLB(pmap->context, vaddr);
	}

	hal_spinlockClear(&pmap_common.lock, &sc);

	return EOK;
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	u8 idx1 = PDIR1_IDX(vaddr), idx2 = PDIR2_IDX(vaddr), idx3 = PDIR3_IDX(vaddr);
	addr_t addr, descr;
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);

	descr = pmap->pdir1[idx1];

	if ((descr & 0x3) == PAGE_INVALID) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return EOK;
	}

	addr = PTD_TO_ADDR(descr);
	descr = hal_cpuLoadPaddr(&((u32 *)addr)[idx2]);

	if ((descr & 0x3) == PAGE_INVALID) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return EOK;
	}

	addr = PTD_TO_ADDR(descr);
	hal_cpuStorePaddr(&((u32 *)addr)[idx3], 0);
	hal_cpuflushDCache();

	_pmap_flushTLB(pmap->context, vaddr);

	hal_spinlockClear(&pmap_common.lock, &sc);

	return EOK;
}


int pmap_getPage(page_t *page, addr_t *addr)
{
	addr_t a, end, min, max, ptable, stack;
	spinlock_ctx_t sc;
	const syspage_prog_t *prog;

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
	(*addr) = a + SIZE_PAGE;

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


	if (page->addr >= min + (4 * 1024 * 1024)) {
		page->flags = PAGE_FREE;
		return EOK;
	}

	page->flags = PAGE_OWNER_KERNEL;
	stack = (addr_t)pmap_common.stack;

	if ((page->addr >= stack) && (page->addr < stack + SIZE_KSTACK)) {
		page->flags |= PAGE_KERNEL_STACK;
		return EOK;
	}

	end = ((addr_t)&_end + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1);
	end += SIZE_EXTEND_BSS;
	if (page->addr >= end - VADDR_KERNEL + min) {
		page->flags |= PAGE_FREE;
		return EOK;
	}

	ptable = (addr_t)pmap_common.ctxTable - VADDR_KERNEL + min;
	end = (addr_t)pmap_common.heap - VADDR_KERNEL + min;
	if ((page->addr >= ptable) && (page->addr < end)) {
		page->flags |= PAGE_KERNEL_PTABLE;
		return EOK;
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
		if (pmap_enter(pmap, 0, vaddr, ~PGHD_PRESENT, NULL) < 0) {
			if (pmap_enter(pmap, 0, vaddr, ~PGHD_PRESENT, dp) < 0) {
				return -ENOMEM;
			}
			dp = NULL;
		}
		(*start) = vaddr;
	}

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


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	ptr_t i;
	u32 pdir2, pdir3, pte;

	hal_memset(pmap_common.ctxMap, 0xff, sizeof(pmap_common.ctxMap));
	/* Context 255 is reserved as shared */
	pmap_common.ctxMap[7] &= ~(1 << 31);
	pmap_common.numCtxFree = MAX_CONTEXTS - 1;

	/* Allocate context for kernel */
	pmap->context = _pmap_contextAlloc();

	hal_spinlockCreate(&pmap_common.lock, "pmap_common.lock");

	pmap_common.minAddr = ADDR_SRAM;
	pmap_common.maxAddr = ADDR_SRAM + SIZE_SRAM;

	/* Initialize kernel page table */
	pmap->pdir1 = pmap_common.pdir1;
	pmap->addr = (addr_t)pmap->pdir1 - VADDR_KERNEL + pmap_common.minAddr;

	/* Remove initial kernel mapping */
	pmap->pdir1[PDIR1_IDX(pmap_common.minAddr)] = 0;

	/* Map kernel text & rodata as RX */
	for (i = VADDR_KERNEL; i < CEIL_PAGE((ptr_t)(&__bss_start)); i += SIZE_PAGE) {
		pdir2 = PTD_TO_ADDR(pmap->pdir1[PDIR1_IDX(i)]);
		pdir3 = PTD_TO_ADDR(hal_cpuLoadPaddr(&((u32 *)pdir2)[PDIR2_IDX(i)]));
		pte = hal_cpuLoadPaddr(&((u32 *)pdir3)[PDIR3_IDX(i)]);
		pte &= ~(0x7 << 2);
		pte |= (PERM_SUPER_RX << 2);
		hal_cpuStorePaddr(&((u32 *)pdir3)[PDIR3_IDX(i)], pte);
		hal_cpuflushDCache();
	}

	/* Map kernel bss and copied syspage as RW */
	for (i = CEIL_PAGE((ptr_t)(&__bss_start)); i < CEIL_PAGE((ptr_t)(&_end)) + SIZE_PAGE; i += SIZE_PAGE) {
		pdir2 = PTD_TO_ADDR(pmap->pdir1[PDIR1_IDX(i)]);
		pdir3 = PTD_TO_ADDR(hal_cpuLoadPaddr(&((u32 *)pdir2)[PDIR2_IDX(i)]));
		pte = hal_cpuLoadPaddr(&((u32 *)pdir3)[PDIR3_IDX(i)]);
		pte &= ~(0x7 << 2);
		pte |= (PERM_SUPER_RW << 2);
		hal_cpuStorePaddr(&((u32 *)pdir3)[PDIR3_IDX(i)], pte);
		hal_cpuflushDCache();
	}
	hal_srmmuFlushTLB(0, TLB_FLUSH_ALL);

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	/* Initialize kernel heap start address */
	(*vstart) = (void *)CEIL_PAGE((u32)&_end);

	/* Skip copied syspage and mapped peripherals */
	(*vstart) += SIZE_EXTEND_BSS;
	(*vend) = (*vstart) + SIZE_PAGE;

	pmap_common.start = (u32)pmap_common.heap - VADDR_KERNEL + pmap_common.minAddr;
	pmap_common.end = pmap_common.start + SIZE_PAGE;

	/* Create initial heap */
	pmap_enter(pmap, (addr_t)pmap_common.start, (*vstart), PGHD_WRITE | PGHD_READ | PGHD_PRESENT, NULL);
}
