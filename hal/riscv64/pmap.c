/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem (RISCV64)
 *
 * Copyright 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Julia Kosowska, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include "hal/pmap.h"
#include "hal/hal.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "halsyspage.h"

#include "dtb.h"
#include "riscv64.h"

#include "include/errno.h"
#include "include/mman.h"

#include <board_config.h>


#define PDIR2_IDX(va) (((ptr_t)(va) >> 30) & 0x1ffU)
#define PDIR1_IDX(va) (((ptr_t)(va) >> 21) & 0x1ffU)
#define PDIR0_IDX(va) (((ptr_t)(va) >> 12) & 0x1ffU)

#define PTE(paddr, flags) (((addr_t)(paddr) >> 12) << 10 | (unsigned int)(flags))
#define PTE_TO_ADDR(pte)  ((((u64)(pte) >> 10) << 12) & 0xfffffffffff000UL)

/* PTE attributes */
#define PTE_V (1UL << 0)

#define CEIL_PAGE(x) ((((addr_t)(x)) + SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U))

/* Linker symbols */
/* parasoft-begin-suppress MISRAC2012-RULE_8_6 "Provided by toolchain" */
extern unsigned int _start;
extern unsigned int _end;
extern unsigned int _etext;
/* parasoft-end-suppress MISRAC2012-RULE_8_6 */


#define PMAP_MEM_ENTRIES 64


typedef struct {
	addr_t start;
	size_t pageCount;
	int flags;
} pmap_memEntry_t;


struct {
	/* The order of below fields should be strictly preserved */
	u64 pdir2[512];
	u64 pdir1[512];
	u64 pdir0[512];

	u8 stack[MAX_CPU_COUNT][SIZE_INITIAL_KSTACK];
	u8 heap[SIZE_PAGE];

	addr_t ptable[SIZE_PAGE / sizeof(addr_t)]; /* ptable used for mapping pages */

	/* The order of below fields could be randomized */
	addr_t minAddr;
	addr_t maxAddr;

	ptr_t start;
	ptr_t end;
	spinlock_t lock;

	u64 dtb;
	u32 dtbsz;

	addr_t kernel;
	size_t kernelsz;
	ptr_t vkernelEnd;

	struct {
		pmap_memEntry_t entries[PMAP_MEM_ENTRIES];
		size_t count;
	} memMap;
	addr_t pageIterator;
	/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 MISRAC2012-RULE_1_1 "Symbol used in assembly, theres no limits" */
} __attribute__((aligned(SIZE_PAGE))) pmap_common;


addr_t pmap_getKernelStart(void)
{
	return pmap_common.kernel;
}


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	unsigned int i, pages;
	ptr_t va;

	pmap->pdir2 = vaddr;
	pmap->satp = (p->addr >> 12) | SATP_MODE_SV39;

	/* Copy kernel page tables */
	hal_memset(pmap->pdir2, 0, 4096);

	kpmap->end = (void *)(((ptr_t)kpmap->end + (ptr_t)(SIZE_PAGE << 18) - 1U) & ~(((ptr_t)SIZE_PAGE << 18) - 1U));

	va = ((ptr_t)kpmap->start & ~(((ptr_t)SIZE_PAGE << 18) - 1U));
	pages = (unsigned int)(((ptr_t)kpmap->end - va) / ((ptr_t)SIZE_PAGE << 18));

	for (i = 0; i < pages; ++i) {
		pmap->pdir2[PDIR2_IDX(va)] = kpmap->pdir2[PDIR2_IDX(va)];
		va += (ptr_t)(SIZE_PAGE << 18);
	}

	pmap->pdir2[511] = kpmap->pdir2[511];

	RISCV_FENCE(rw, rw);
	hal_cpuInstrBarrier();

	return EOK;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	const int idx2 = (int)PDIR2_IDX(VADDR_KERNEL);
	addr_t pdir1, entry;
	size_t j;
	spinlock_ctx_t sc;


	while (*i < idx2) {
		entry = pmap->pdir2[*i];
		if ((entry & PTE_V) != 0U) {
			pdir1 = PTE_TO_ADDR(entry);
			hal_spinlockSet(&pmap_common.lock, &sc);

			/* Map pdir0 into scratch ptable */
			pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(pdir1, 0xc7);
			hal_cpuLocalFlushTLB(0, pmap_common.ptable);
			hal_cpuDCacheInval(pmap_common.ptable, sizeof(pmap_common.ptable));

			for (j = 0; j < 512U; j++) {
				entry = pmap_common.ptable[j];
				if ((entry & PTE_V) != 0U) {
					pmap_common.ptable[j] = 0;
					hal_spinlockClear(&pmap_common.lock, &sc);

					return PTE_TO_ADDR(entry);
				}
			}
			hal_spinlockClear(&pmap_common.lock, &sc);

			pmap->pdir2[*i] = 0;

			(*i)++;

			return pdir1;
		}
		(*i)++;
	}

	return 0;
}


void pmap_switch(pmap_t *pmap)
{
	hal_cpuSwitchSpace(pmap->satp);
}


static int _pmap_map(u64 *pdir2, u64 satpVal, addr_t pa, void *vaddr, int attr, page_t *alloc)
{
	addr_t addr;

	/* Returned from macro only 9 LSB */
	unsigned long pdi2 = PDIR2_IDX(vaddr);
	unsigned long pdi1 = PDIR1_IDX(vaddr);
	unsigned long pti = PDIR0_IDX(vaddr);

	u64 curSatp;

	if (((unsigned int)attr & PGHD_WRITE) != 0U) {
		/* RISC-V ISA: w/wx mapping reserved for future use */
		attr = (int)(unsigned int)((unsigned int)attr | PGHD_READ);
	}

	/* If no page table is allocated add new one */
	if ((pdir2[pdi2] & PTE_V) == 0U) {
		if (alloc == NULL) {
			return -EFAULT;
		}

		pdir2[pdi2] = PTE(alloc->addr, PTE_V);

		/* Initialize pdir (MOD) - because of reentrancy */
		pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(alloc->addr, 0xc7);
		hal_cpuLocalFlushTLB(0, pmap_common.ptable);
		hal_memset(pmap_common.ptable, 0, sizeof(pmap_common.ptable));

		alloc = NULL;
	}
	else {
		/* Map next level pdir */
		addr = PTE_TO_ADDR(pdir2[pdi2]);
		pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
		hal_cpuLocalFlushTLB(0, pmap_common.ptable);
		hal_cpuDCacheInval(pmap_common.ptable, sizeof(pmap_common.ptable));
	}

	if ((pmap_common.ptable[pdi1] & PTE_V) == 0U) {
		if (alloc == NULL) {
			return -EFAULT;
		}
		pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(alloc->addr, 0xc7);
		hal_cpuLocalFlushTLB(0, pmap_common.ptable);
		hal_memset(pmap_common.ptable, 0, sizeof(pmap_common.ptable));

		addr = PTE_TO_ADDR(pdir2[pdi2]);
		pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
		hal_cpuLocalFlushTLB(0, pmap_common.ptable);

		pmap_common.ptable[pdi1] = PTE(alloc->addr, PTE_V);
	}

	/* Map next level pdir */
	addr = PTE_TO_ADDR(pmap_common.ptable[pdi1]);
	pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
	hal_cpuLocalFlushTLB(0, pmap_common.ptable);

	if ((pmap_common.ptable[pti] & (PGHD_WRITE | PTE_V)) == (PGHD_WRITE | PTE_V)) {
		curSatp = csr_read(satp);
		if ((ptr_t)vaddr < VADDR_USR_MAX) {
			hal_cpuSwitchSpace(satpVal);
		}

		hal_cpuDCacheFlush(vaddr, SIZE_PAGE);
		hal_cpuDCacheInval(vaddr, SIZE_PAGE);

		if ((ptr_t)vaddr < VADDR_USR_MAX) {
			hal_cpuSwitchSpace(curSatp);
		}
	}

	/* And at last map page or only changle attributes of map entry */
	pmap_common.ptable[pti] = PTE(pa, 0xc1U | ((unsigned int)attr & 0x3fU));

	RISCV_FENCE(w, rw);

	return EOK;
}


static int _pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, vm_attr_t attr, page_t *alloc, int tlbInval)
{
	u64 satpVal;
	int ret = _pmap_map(pmap->pdir2, pmap->satp, pa, vaddr, attr, alloc);
	if (ret < 0) {
		return ret;
	}

	if (tlbInval != 0) {
		hal_cpuRemoteFlushTLB(0, vaddr, SIZE_PAGE);
	}
	else {
		hal_cpuLocalFlushTLB(0, vaddr);
	}

	if (((unsigned int)attr & PGHD_WRITE) != 0U) {
		satpVal = csr_read(satp);
		if ((ptr_t)vaddr <= VADDR_USR_MAX) {
			hal_cpuSwitchSpace(pmap->satp);
		}

		hal_cpuDCacheInval(vaddr, SIZE_PAGE);

		if ((ptr_t)vaddr <= VADDR_USR_MAX) {
			hal_cpuSwitchSpace(satpVal);
		}
	}
	hal_cpuInstrBarrier();
	if (((unsigned int)attr & (unsigned int)PGHD_EXEC) != 0U) {
		hal_cpuRfenceI();
	}

	return EOK;
}


/* Functions maps page at specified address (Sv39) */
int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, vm_attr_t attr, page_t *alloc)
{
	int ret;
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);
	ret = _pmap_enter(pmap, pa, vaddr, attr, alloc, 1);
	hal_spinlockClear(&pmap_common.lock, &sc);

	return ret;
}


static u8 _pmap_remove(pmap_t *pmap, void *vstart, void *vend)
{
	addr_t addr, entry;
	ptr_t vaddr;

	unsigned long pdi2, pdi1, pti;
	int foundttl0 = 0;

	u64 satpVal;
	u8 isync = 0;

	for (vaddr = (ptr_t)vstart; vaddr < (ptr_t)vend; vaddr += SIZE_PAGE) {
		pdi2 = PDIR2_IDX(vaddr);
		pdi1 = PDIR1_IDX(vaddr);
		pti = PDIR0_IDX(vaddr);

		if ((foundttl0 == 0) || (pti == 0U)) {
			foundttl0 = 0; /* Set when pti = 0 */
			entry = pmap->pdir2[pdi2];
			if ((entry & PTE_V) == 0U) {
				continue;
			}

			addr = PTE_TO_ADDR(entry);

			pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
			hal_cpuLocalFlushTLB(0, pmap_common.ptable);
			hal_cpuDCacheInval(pmap_common.ptable, sizeof(pmap_common.ptable));

			entry = pmap_common.ptable[pdi1];
			if ((entry & PTE_V) == 0U) {
				continue;
			}

			addr = PTE_TO_ADDR(entry);
			pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
			hal_cpuLocalFlushTLB(0, pmap_common.ptable);
			hal_cpuDCacheInval(pmap_common.ptable, sizeof(pmap_common.ptable));

			foundttl0 = 1;
		}

		entry = pmap_common.ptable[pti];

		if ((entry & (unsigned int)PGHD_EXEC) != 0U) {
			isync = 1;
		}

		if ((entry & (PGHD_WRITE | PTE_V)) == (PGHD_WRITE | PTE_V)) {
			satpVal = csr_read(satp);
			if ((ptr_t)vaddr < VADDR_USR_MAX) {
				hal_cpuSwitchSpace(pmap->satp);
			}

			hal_cpuDCacheFlush((void *)vaddr, SIZE_PAGE);
			hal_cpuDCacheInval((void *)vaddr, SIZE_PAGE);

			if ((ptr_t)vaddr < VADDR_USR_MAX) {
				hal_cpuSwitchSpace(satpVal);
			}
		}

		pmap_common.ptable[pti] = 0;
	}

	RISCV_FENCE(w, rw);

	return isync;
}


int pmap_remove(pmap_t *pmap, void *vstart, void *vend)
{
	spinlock_ctx_t sc;
	u8 isync;

	hal_spinlockSet(&pmap_common.lock, &sc);
	isync = _pmap_remove(pmap, vstart, vend);
	hal_cpuRemoteFlushTLB(0, vstart, (size_t)((ptr_t)vend - (ptr_t)vstart));
	hal_cpuInstrBarrier();

	if (isync != 0U) {
		hal_cpuRfenceI();
	}
	hal_spinlockClear(&pmap_common.lock, &sc);

	return EOK;
}


/* Functions returns physical address associated with specified virtual address */
addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	addr_t addr;
	spinlock_ctx_t sc;

	unsigned long pdi2 = PDIR2_IDX(vaddr);
	unsigned long pdi1 = PDIR1_IDX(vaddr);
	unsigned long pti = PDIR0_IDX(vaddr);

	if ((pmap->pdir2[pdi2] & PTE_V) == 0U) {
		return 0;
	}

	hal_spinlockSet(&pmap_common.lock, &sc);

	/* Map page table corresponding to vaddr at specified virtual address */
	addr = PTE_TO_ADDR(pmap->pdir2[pdi2]);

	pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
	hal_cpuLocalFlushTLB(0, pmap_common.ptable);
	hal_cpuDCacheInval(pmap_common.ptable, sizeof(pmap_common.ptable));

	addr = PTE_TO_ADDR(pmap_common.ptable[pdi1]);

	pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
	hal_cpuLocalFlushTLB(0, pmap_common.ptable);
	hal_cpuDCacheInval(pmap_common.ptable, sizeof(pmap_common.ptable));

	addr = PTE_TO_ADDR(pmap_common.ptable[pti]);
	hal_spinlockClear(&pmap_common.lock, &sc);

	return addr;
}


/* Function fills page_t structure for frame given by addr */
int pmap_getPage(page_t *page, addr_t *addr)
{
	size_t i;
	addr_t a;
	spinlock_ctx_t sc;
	const syspage_prog_t *prog;
	const pmap_memEntry_t *entry;

	a = *addr & ~(SIZE_PAGE - 1U);
	page->flags = 0;

	/* Test address ranges */
	hal_spinlockSet(&pmap_common.lock, &sc);

	/* Ignore SBI area */
	if (((a >= SBI_AREA_START) && (a < SBI_AREA_END)) || (a < pmap_common.minAddr)) {
		a = SBI_AREA_END;
	}

	if (a >= pmap_common.maxAddr) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return -ENOMEM;
	}

	hal_spinlockClear(&pmap_common.lock, &sc);

	page->addr = a;
	page->flags = 0;
	*addr = a + SIZE_PAGE;

	for (i = 0; i < pmap_common.memMap.count; i++) {
		entry = &pmap_common.memMap.entries[i];
		if ((a >= entry->start) && ((a - entry->start) < (entry->pageCount * SIZE_PAGE))) {
			page->flags = (u8)entry->flags;
			return EOK;
		}
	}

	if (hal_syspage->progs != NULL) {
		prog = hal_syspage->progs;
		do {
			if (page->addr >= prog->start && page->addr < prog->end) {
				page->flags = PAGE_OWNER_APP;
				return EOK;
			}
			prog = prog->next;
		} while (prog != hal_syspage->progs);
	}

	if ((page->addr >= pmap_common.kernel) && (page->addr < pmap_common.kernel + pmap_common.kernelsz)) {
		page->flags |= PAGE_OWNER_KERNEL;

		if ((page->addr >= (ptr_t)pmap_common.pdir2) && (page->addr < ((ptr_t)pmap_common.pdir2 + 3U * SIZE_PAGE))) {
			page->flags |= PAGE_KERNEL_PTABLE;
		}

		if ((page->addr >= (ptr_t)pmap_common.ptable) && (page->addr < ((ptr_t)pmap_common.ptable + sizeof(pmap_common.ptable)))) {
			page->flags |= PAGE_KERNEL_PTABLE;
		}

		if ((page->addr >= (ptr_t)pmap_common.stack) && (page->addr < ((ptr_t)pmap_common.stack + sizeof(pmap_common.stack)))) {
			page->flags |= PAGE_KERNEL_STACK;
		}
	}
	else if ((page->addr >= pmap_common.dtb) && (page->addr < (pmap_common.dtb + pmap_common.dtbsz))) {
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
	void *vaddr;

	vaddr = (void *)((ptr_t)(*start + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1U));

	if ((ptr_t)vaddr >= (ptr_t)end) {
		return EOK;
	}

	if (vaddr < (void *)VADDR_KERNEL) {
		vaddr = (void *)VADDR_KERNEL;
	}


	for (; vaddr < end; vaddr += (1ULL << 30)) {
		if (_pmap_enter(pmap, 0, vaddr, (int)(~PGHD_PRESENT), NULL, 0) < 0) {
			if (_pmap_enter(pmap, 0, vaddr, (int)(~PGHD_PRESENT), dp, 0) < 0) {
				return -ENOMEM;
			}
			dp = NULL;
		}
		*start = vaddr;
	}
	hal_cpuLocalFlushTLB(0, NULL);

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = end;

	return EOK;
}


/* Function return character marker for page flags */
char pmap_marker(page_t *p)
{
	const char *marksets[4] = { "BBBBBBBBBBBBBBBB", "KYCPMSHKKKKKKKKK", "AAAAAAAAAAAAAAAA", "UUUUUUUUUUUUUUUU" };

	if ((p->flags & PAGE_FREE) != 0U) {
		return '.';
	}

	return marksets[(p->flags >> 1) & 3U][(p->flags >> 4) & 0xfU];
}


int pmap_segment(unsigned int i, void **vaddr, size_t *size, vm_prot_t *prot, void **top)
{
	switch (i) {
		case 0:
			*vaddr = (void *)VADDR_KERNEL;
			*size = (ptr_t)&_etext - VADDR_KERNEL;
			*prot = (int)(unsigned int)(PROT_EXEC | PROT_READ);
			break;
		case 1:
			*vaddr = &_etext;
			*size = (ptr_t)(*top) - (ptr_t)&_etext;
			*prot = (int)(unsigned int)(PROT_WRITE | PROT_READ);
			break;
		default:
			return -EINVAL;
	}

	return EOK;
}


static int _pmap_addMemEntry(addr_t start, size_t length, unsigned int flags)
{
	addr_t end;
	size_t pageCount;

	if (pmap_common.memMap.count >= (unsigned long)PMAP_MEM_ENTRIES) {
		return -ENOMEM;
	}

	end = CEIL_PAGE(start + length);
	pageCount = (end - start) / SIZE_PAGE;

	pmap_common.memMap.entries[pmap_common.memMap.count].start = start;
	pmap_common.memMap.entries[pmap_common.memMap.count].pageCount = pageCount;
	pmap_common.memMap.entries[pmap_common.memMap.count].flags = (int)flags;

	pmap_common.memMap.count++;
	return EOK;
}


static int _pmap_findFreePage(page_t *page)
{
	int ret = -ENOMEM;

	while (pmap_common.pageIterator < pmap_common.maxAddr) {
		ret = pmap_getPage(page, &pmap_common.pageIterator);
		if (((page->flags & PAGE_FREE) != 0U) || (ret != EOK)) {
			break;
		}
	}

	return ret;
}


static void *_pmap_halMapInternal(addr_t paddr, void *va, size_t size, int attr, int remoteFlush)
{
	void *baseVa;
	void **pva;
	addr_t end;
	page_t *alloc = NULL;
	page_t page;
	u64 satpVal;

	if ((hal_started() != 0) && (va == NULL)) {
		return NULL;
	}

	paddr &= ~(SIZE_PAGE - 1U);
	end = CEIL_PAGE(paddr + size);

	/* Handle overflow, but allow mapping to the end of the physical address space (end = 0) */
	if ((end != 0U) && (end < paddr)) {
		return NULL;
	}

	if (va == NULL) {
		pva = (void **)&pmap_common.vkernelEnd;
		va = *pva;
	}
	else {
		va = (void *)((ptr_t)va & ~(SIZE_PAGE - 1U));
		pva = &va;
	}

	baseVa = va;

	satpVal = csr_read(satp);

	while (paddr != end) {
		while (_pmap_map(pmap_common.pdir2, satpVal, paddr, *pva, attr, alloc) < 0) {
			if (_pmap_findFreePage(&page) < 0) {
				if (remoteFlush != 0) {
					hal_cpuRemoteFlushTLB(0, baseVa, (ptr_t)*pva - (ptr_t)baseVa);
				}
				return NULL;
			}
			if (_pmap_addMemEntry(page.addr, SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE) != EOK) {
				if (remoteFlush != 0) {
					hal_cpuRemoteFlushTLB(0, baseVa, (ptr_t)*pva - (ptr_t)baseVa);
				}
				return NULL;
			}
			alloc = &page;
		}

		if (remoteFlush == 0) {
			hal_cpuLocalFlushTLB(0, *pva);
		}
		alloc = NULL;
		*pva += SIZE_PAGE;
		paddr += SIZE_PAGE;
	}

	if (remoteFlush != 0) {
		hal_cpuRemoteFlushTLB(0, baseVa, size);
	}

	return baseVa;
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
	hal_spinlockClear(&pmap_common.lock, &sc);

	return ret;
}


void *_pmap_halMapDevice(addr_t paddr, size_t pageOffs, size_t size)
{
	void *ret;

	ret = _pmap_halMap(paddr, NULL, size, (int)(PGHD_WRITE | PGHD_READ | PGHD_DEV | PGHD_PRESENT));

	return (void *)((ptr_t)ret + pageOffs);
}


/* Function initializes low-level page mapping interface */
void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	void *v;

	/* Initialize kernel page table - remove first 4 MB mapping */
	pmap->pdir2 = pmap_common.pdir2;

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	/* Initialize kernel heap start address */
	(*vstart) = (void *)pmap_common.vkernelEnd;

	(*vend) = (*vstart) + SIZE_PAGE;

	pmap_common.start = (ptr_t)pmap_common.heap - VADDR_KERNEL + pmap_common.kernel;
	pmap_common.end = pmap_common.start + SIZE_PAGE;

	/* Create initial heap */
	(void)_pmap_enter(pmap, pmap_common.start, (*vstart), (int)(PGHD_READ | PGHD_WRITE | PGHD_PRESENT), NULL, 0);

	/* Remove initial mapping */
	(void)_pmap_remove(pmap, *vend, (void *)(VADDR_KERNEL + (2UL << 20)));

	/* Map kernel text as RX */
	for (v = (void *)VADDR_KERNEL; v < (void *)CEIL_PAGE((ptr_t)(&_etext)); v = (char *)v + SIZE_PAGE) {
		(void)_pmap_enter(pmap, pmap_resolve(pmap, v), v, (int)(PGHD_READ | PGHD_EXEC | PGHD_PRESENT), NULL, 0);
	}

	/* Map everything else as RW */
	for (v = (void *)CEIL_PAGE((ptr_t)(&_etext)); v < (void *)(CEIL_PAGE((ptr_t)&_end)); v = (char *)v + SIZE_PAGE) {
		(void)_pmap_enter(pmap, pmap_resolve(pmap, v), v, (int)(PGHD_READ | PGHD_WRITE | PGHD_PRESENT), NULL, 0);
	}

	pmap->satp = ((pmap_resolve(pmap, (char *)pmap_common.pdir2) >> 12) | SATP_MODE_SV39);

	hal_cpuLocalFlushTLB(0, NULL);
}


void _pmap_halInit(void)
{
	struct {
		u8 addr[8];
		u8 limit[8];
	} *m;
	size_t n, i;
	u64 a, l;

	dtb_getMemory((u8 **)&m, &n);
	dtb_getDTBArea(&pmap_common.dtb, &pmap_common.dtbsz);

	hal_spinlockCreate(&pmap_common.lock, "pmap_common.lock");

	/* Calculate physical address space range */
	pmap_common.minAddr = (u64)-1;
	pmap_common.maxAddr = 0;

	for (i = 0; i < n; i++) {
		hal_memcpy(&a, &m[i].addr, sizeof(a));
		hal_memcpy(&l, &m[i].limit, sizeof(l));
		a = ntoh64(a);
		l = ntoh64(l);

		if ((a + l) > pmap_common.maxAddr) {
			pmap_common.maxAddr = a + l;
		}
		if (a < pmap_common.minAddr) {
			pmap_common.minAddr = a;
		}
	}

	pmap_common.pageIterator = pmap_common.minAddr;
	pmap_common.memMap.count = 0;
	pmap_common.kernelsz = CEIL_PAGE(&_end) - (addr_t)VADDR_KERNEL;
	pmap_common.vkernelEnd = VADDR_KERNEL + pmap_common.kernelsz;
}


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Definition in assembly" */
void _pmap_preinit(addr_t dtb)
{
	unsigned int i;

	/* Get physical kernel address */
	pmap_common.kernel = (addr_t)&_start;

	hal_memset(pmap_common.pdir0, 0, SIZE_PAGE);
	hal_memset(pmap_common.pdir1, 0, SIZE_PAGE);
	hal_memset(pmap_common.pdir2, 0, SIZE_PAGE);

	/* Map 4MB after _start symbol at VADDR_KERNEL */
	pmap_common.pdir2[(VADDR_KERNEL >> 30) % 512U] = ((addr_t)pmap_common.pdir1 >> 2) | PTE_V;
	pmap_common.pdir1[(VADDR_KERNEL >> 21) % 512U] = ((addr_t)pmap_common.pdir0 >> 2) | PTE_V;

	for (i = 0; i < 512U; i++) {
		pmap_common.pdir0[((VADDR_KERNEL >> 12) % 512U) + i] = PTE((addr_t)&_start + i * SIZE_PAGE, 0xcf);
	}

	/* Map physical memory to reach DTB before vm subsystem initialization (MOD) */
	pmap_common.pdir2[511] = (((dtb >> 30) << 28) | 0xc3U);
}
