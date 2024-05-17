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

#include <arch/tlb.h>

#include "hal/pmap.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "hal/tlb/tlb.h"
#include "halsyspage.h"

#include "dtb.h"
#include "riscv64.h"

#include "include/errno.h"
#include "include/mman.h"


#define PDIR2_IDX(va) (((ptr_t)(va) >> 30) & 0x1ffUL)
#define PDIR1_IDX(va) (((ptr_t)(va) >> 21) & 0x1ffUL)
#define PDIR0_IDX(va) (((ptr_t)(va) >> 12) & 0x1ffUL)

#define PTE(paddr, flags) (((addr_t)(paddr) >> 12) << 10 | (flags))
#define PTE_TO_ADDR(pte)  ((((u64)(pte) >> 10) << 12) & 0xfffffffffff000UL)

/* PTE attributes */
#define PTE_V (1UL << 0)

#define CEIL_PAGE(x) (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))

/* Linker symbols */
extern unsigned int _start;
extern unsigned int _end;
extern unsigned int _etext;


struct {
	/* The order of below fields should be stricly preserved */
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

	kpmap->end = (void *)(((ptr_t)kpmap->end + (ptr_t)(SIZE_PAGE << 18) - 1) & ~(((ptr_t)SIZE_PAGE << 18) - 1));

	va = ((ptr_t)kpmap->start & ~(((ptr_t)SIZE_PAGE << 18) - 1));
	pages = ((ptr_t)kpmap->end - va) / ((ptr_t)SIZE_PAGE << 18);

	for (i = 0; i < pages; va += (ptr_t)(SIZE_PAGE << 18), ++i) {
		pmap->pdir2[PDIR2_IDX(va)] = kpmap->pdir2[PDIR2_IDX(va)];
	}

	pmap->pdir2[511] = kpmap->pdir2[511];

	RISCV_FENCE(rw, rw);
	hal_cpuInstrBarrier();

	return EOK;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	int kernel = VADDR_KERNEL / ((u64)SIZE_PAGE << 18);

	while (*i < kernel) {
		if (pmap->pdir2[*i] != NULL) {
			return (pmap->pdir2[(*i)++] & (u64)~0x3ff) << 2;
		}
		(*i)++;
	}

	return 0;
}


void pmap_switch(pmap_t *pmap)
{
	hal_cpuSwitchSpace(pmap->satp);
}


static int _pmap_enter(pmap_t *pmap, addr_t pa, void *va, int attr, page_t *alloc, int tlbInval)
{
	addr_t addr;

	unsigned int pdi2 = PDIR2_IDX(va);
	unsigned int pdi1 = PDIR1_IDX(va);
	unsigned int pti = PDIR0_IDX(va);

	/* If no page table is allocated add new one */
	if ((pmap->pdir2[pdi2] & PTE_V) == 0) {
		if (alloc == NULL) {
			return -EFAULT;
		}

		pmap->pdir2[pdi2] = PTE(alloc->addr, PTE_V);

		/* Initialize pdir (MOD) - because of reentrancy */
		pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(alloc->addr, 0xc7);
		hal_tlbInvalidateLocalEntry(NULL, pmap_common.ptable);
		hal_memset(pmap_common.ptable, 0, sizeof(pmap_common.ptable));

		alloc = NULL;
	}
	else {
		/* Map next level pdir */
		addr = PTE_TO_ADDR(pmap->pdir2[pdi2]);
		pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
		hal_tlbInvalidateLocalEntry(NULL, pmap_common.ptable);
	}

	if ((pmap_common.ptable[pdi1] & PTE_V) == 0) {
		if (alloc == NULL) {
			return -EFAULT;
		}
		pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(alloc->addr, 0xc7);
		hal_tlbInvalidateLocalEntry(NULL, pmap_common.ptable);
		hal_memset(pmap_common.ptable, 0, sizeof(pmap_common.ptable));

		addr = PTE_TO_ADDR(pmap->pdir2[pdi2]);
		pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
		hal_tlbInvalidateLocalEntry(NULL, pmap_common.ptable);

		pmap_common.ptable[pdi1] = PTE(alloc->addr, PTE_V);

		alloc = NULL;
	}

	/* Map next level pdir */
	addr = PTE_TO_ADDR(pmap_common.ptable[pdi1]);
	pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
	hal_tlbInvalidateLocalEntry(NULL, pmap_common.ptable);

	/* And at last map page or only changle attributes of map entry */
	pmap_common.ptable[pti] = PTE(pa, 0xc1 | (attr & 0x3f));

	RISCV_FENCE(rw, rw);
	hal_cpuInstrBarrier();
	if ((attr & PGHD_EXEC) != 0) {
		hal_cpuRfenceI();
	}

	if (tlbInval != 0) {
		hal_tlbInvalidateEntry(pmap, va, 1);
	}
	else {
		hal_tlbInvalidateLocalEntry(pmap, va);
	}

	return EOK;
}


/* Functions maps page at specified address (Sv39) */
int pmap_enter(pmap_t *pmap, addr_t pa, void *va, int attr, page_t *alloc)
{
	int ret;
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);
	ret = _pmap_enter(pmap, pa, va, attr, alloc, 1);
	if (ret == EOK) {
		hal_tlbCommit(&pmap_common.lock, &sc);
	}
	else {
		hal_spinlockClear(&pmap_common.lock, &sc);
	}
	return ret;
}


static void _pmap_remove(pmap_t *pmap, void *vaddr)
{
	addr_t addr, entry;

	unsigned int pdi2 = PDIR2_IDX(vaddr);
	unsigned int pdi1 = PDIR1_IDX(vaddr);
	unsigned int pti = PDIR0_IDX(vaddr);

	u8 isync = 0;

	entry = pmap->pdir2[pdi2];
	if ((entry & PTE_V) == 0) {
		return;
	}

	addr = PTE_TO_ADDR(entry);

	pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
	hal_tlbInvalidateLocalEntry(NULL, pmap_common.ptable);

	entry = pmap_common.ptable[pdi1];

	if ((entry & PTE_V) != 0) {
		addr = PTE_TO_ADDR(entry);
		pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
		hal_tlbInvalidateLocalEntry(NULL, pmap_common.ptable);

		if ((pmap_common.ptable[pti] & PGHD_EXEC) != 0) {
			isync = 1;
		}

		pmap_common.ptable[pti] = 0;
	}

	RISCV_FENCE(rw, rw);
	hal_cpuInstrBarrier();

	if (isync != 0) {
		hal_cpuRfenceI();
	}
}


int pmap_remove(pmap_t *pmap, void *vaddr)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&pmap_common.lock, &sc);
	_pmap_remove(pmap, vaddr);
	hal_tlbInvalidateEntry(pmap, vaddr, 1);
	hal_tlbCommit(&pmap_common.lock, &sc);

	return EOK;
}


/* Functions returs physical address associated with specified virtual address */
addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	addr_t addr;
	spinlock_ctx_t sc;

	unsigned int pdi2 = PDIR2_IDX(vaddr);
	unsigned int pdi1 = PDIR1_IDX(vaddr);
	unsigned int pti = PDIR0_IDX(vaddr);

	if ((pmap->pdir2[pdi2] & PTE_V) == 0) {
		return 0;
	}

	hal_spinlockSet(&pmap_common.lock, &sc);

	/* Map page table corresponding to vaddr at specified virtual address */
	addr = PTE_TO_ADDR(pmap->pdir2[pdi2]);

	pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
	hal_tlbInvalidateLocalEntry(NULL, pmap_common.ptable);

	addr = PTE_TO_ADDR(pmap_common.ptable[pdi1]);

	pmap_common.pdir0[PDIR0_IDX(pmap_common.ptable)] = PTE(addr, 0xc7);
	hal_tlbInvalidateLocalEntry(NULL, pmap_common.ptable);

	addr = PTE_TO_ADDR(pmap_common.ptable[pti]);
	hal_spinlockClear(&pmap_common.lock, &sc);

	return addr;
}


/* Function fills page_t structure for frame given by addr */
int pmap_getPage(page_t *page, addr_t *addr)
{
	addr_t a;
	spinlock_ctx_t sc;
	const syspage_prog_t *prog;

	a = *addr & ~(SIZE_PAGE - 1);
	page->flags = 0;

	/* Test address ranges */
	hal_spinlockSet(&pmap_common.lock, &sc);

	/* Ignore SBI area */
	if (((a >= 0x80000000UL) && (a < 0x80200000UL)) || (a < pmap_common.minAddr)) {
		a = 0x80200000;
	}

	if (a >= pmap_common.maxAddr) {
		hal_spinlockClear(&pmap_common.lock, &sc);
		return -ENOMEM;
	}

	hal_spinlockClear(&pmap_common.lock, &sc);

	page->addr = a;
	page->flags = 0;
	*addr = a + SIZE_PAGE;

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

		if ((page->addr >= (ptr_t)pmap_common.pdir2) && (page->addr < ((ptr_t)pmap_common.pdir2 + 3 * SIZE_PAGE))) {
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

	vaddr = (void *)((ptr_t)(*start + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));

	if (vaddr >= end) {
		return EOK;
	}

	if (vaddr < (void *)VADDR_KERNEL) {
		vaddr = (void *)VADDR_KERNEL;
	}


	for (; vaddr < end; vaddr += (1ULL << 30)) {
		if (_pmap_enter(pmap, 0, vaddr, ~PGHD_PRESENT, NULL, 0) < 0) {
			if (_pmap_enter(pmap, 0, vaddr, ~PGHD_PRESENT, dp, 0) < 0) {
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
	char *marksets[4] = { "BBBBBBBBBBBBBBBB", "KYCPMSHKKKKKKKKK", "AAAAAAAAAAAAAAAA", "UUUUUUUUUUUUUUUU" };

	if (p->flags & PAGE_FREE) {
		return '.';
	}

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

	dtb_getMemory((ptr_t **)&m, &n);
	dtb_getReservedMemory((ptr_t **)&r);
	dtb_getDTBArea(&pmap_common.dtb, &pmap_common.dtbsz);

	hal_spinlockCreate(&pmap_common.lock, "pmap_common.lock");

	/* Calculate physical address space range */
	pmap_common.minAddr = (u64)-1;
	pmap_common.maxAddr = 0;

	for (i = 0; i < n; i++) {
		a = ntoh64(m[i].addr);
		l = ntoh64(m[i].limit);

		if ((a + l) > pmap_common.maxAddr) {
			pmap_common.maxAddr = a + l;
		}
		if (a < pmap_common.minAddr) {
			pmap_common.minAddr = a;
		}
	}

	/* Initialize kernel page table - remove first 4 MB mapping */
	pmap->pdir2 = pmap_common.pdir2;

	pmap->start = (void *)VADDR_KERNEL;
	pmap->end = (void *)VADDR_MAX;

	/* Initialize kernel heap start address */
	(*vstart) = (void *)(((ptr_t)&_end + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));
	(*vstart) += SIZE_PAGE; /* Reserve space for syspage */

	(*vend) = (*vstart) + SIZE_PAGE;

	pmap_common.start = (ptr_t)pmap_common.heap - VADDR_KERNEL + pmap_common.kernel;
	pmap_common.end = pmap_common.start + SIZE_PAGE;

	/* Create initial heap */
	_pmap_enter(pmap, pmap_common.start, (*vstart), PGHD_READ | PGHD_WRITE | PGHD_PRESENT, NULL, 0);

	for (v = *vend; v < (void *)((char *)VADDR_KERNEL + (2 << 20)); v = (char *)v + SIZE_PAGE) {
		_pmap_remove(pmap, v);
	}

	/* Map kernel text as RX */
	for (v = (void *)VADDR_KERNEL; v < (void *)CEIL_PAGE((ptr_t)(&_etext)); v = (char *)v + SIZE_PAGE) {
		_pmap_enter(pmap, pmap_resolve(pmap, v), v, PGHD_READ | PGHD_EXEC | PGHD_PRESENT, NULL, 0);
	}

	/* Map everything else as RW */
	for (v = (void *)CEIL_PAGE((ptr_t)(&_etext)); v < (void *)(CEIL_PAGE((ptr_t)&_end) + SIZE_PAGE); v = (char *)v + SIZE_PAGE) {
		_pmap_enter(pmap, pmap_resolve(pmap, v), v, PGHD_READ | PGHD_WRITE | PGHD_PRESENT, NULL, 0);
	}

	pmap->satp = ((pmap_resolve(pmap, (char *)pmap_common.pdir2) >> 12) | SATP_MODE_SV39);

	hal_tlbFlushLocal(NULL);
}


int pmap_segment(unsigned int i, void **vaddr, size_t *size, int *prot, void **top)
{
	switch (i) {
		case 0:
			*vaddr = (void *)VADDR_KERNEL;
			*size = (ptr_t)&_etext - VADDR_KERNEL;
			*prot = (PROT_EXEC | PROT_READ);
			break;
		case 1:
			*vaddr = &_etext;
			*size = (ptr_t)(*top) - (ptr_t)&_etext;
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

	/* Get physical kernel address */
	pmap_common.kernel = (addr_t)&_start;
	/* Add SIZE_PAGE to kernel size for syspage */
	pmap_common.kernelsz = (((addr_t)&_end + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1)) - (addr_t)&_start + SIZE_PAGE;

	hal_memset(pmap_common.pdir0, 0, SIZE_PAGE);
	hal_memset(pmap_common.pdir1, 0, SIZE_PAGE);
	hal_memset(pmap_common.pdir2, 0, SIZE_PAGE);

	/* Map 4MB after _start symbol at VADDR_KERNEL */
	pmap_common.pdir2[(VADDR_KERNEL >> 30) % 512] = ((addr_t)pmap_common.pdir1 >> 2) | PTE_V;
	pmap_common.pdir1[(VADDR_KERNEL >> 21) % 512] = ((addr_t)pmap_common.pdir0 >> 2) | PTE_V;

	for (i = 0; i < 512; i++) {
		pmap_common.pdir0[((VADDR_KERNEL >> 12) % 512) + i] = PTE((addr_t)&_start + i * SIZE_PAGE, 0xcf);
	}

	/* Map PLIC (MOD) */
	pmap_common.pdir2[511] = 0xc7;

	/* Map physical memory to reach DTB before vm subsystem initialization (MOD) */
	pmap_common.pdir2[510] = ((0x80000000 >> 2) | 0xc7);

	return;
}
