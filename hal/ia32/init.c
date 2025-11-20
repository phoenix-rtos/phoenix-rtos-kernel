/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration detection and other initialisation routines
 *
 * Copyright 2023 Phoenix Systems
 * Author: Andrzej Stalke
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/string.h"
#include "hal/cpu.h"
#include "hal/pmap.h"
#include "init.h"
#include "ia32.h"
#include "include/errno.h"

#include <arch/tlb.h>

/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Provided by toolchain" */
extern unsigned int _end;
extern syspage_t *syspage;
hal_config_t hal_config;
static struct {
	addr_t pageIterator;
} init_common;


static int _hal_addMemEntry(addr_t start, u32 length, u32 flags)
{
	u64 end, pageCount;
	if (hal_config.memMap.count < HAL_MEM_ENTRIES) {
		end = start;
		end += length;
		if ((end % SIZE_PAGE) != 0U) {
			end += SIZE_PAGE;
		}
		end &= ~(SIZE_PAGE - 1U);

		start &= ~(SIZE_PAGE - 1U);
		pageCount = (end - start) / SIZE_PAGE;

		hal_config.memMap.entries[hal_config.memMap.count].start = start;
		hal_config.memMap.entries[hal_config.memMap.count].pageCount = (u32)pageCount;
		hal_config.memMap.entries[hal_config.memMap.count].flags = flags;
		hal_config.memMap.count += 1U;
		return 0;
	}
	else {
		return 1;
	}
}


static inline int _hal_findFreePage(page_t *page)
{
	int ret = -ENOMEM;
	while (init_common.pageIterator < 0xffff0000U) {
		ret = pmap_getPage(page, &init_common.pageIterator);
		if ((ret != EOK) || ((page->flags & PAGE_FREE) != 0U)) {
			break;
		}
	}
	return ret;
}


static inline int _hal_configMapPage(u32 *pdir, addr_t paddr, void *vaddr, vm_attr_t attr)
{
	page_t page = { 0 };
	addr_t *ptable;
	int ret = _pmap_enter(pdir, hal_config.ptable, paddr, vaddr, attr, NULL, 0);
	if (ret < 0) {
		ret = _hal_findFreePage(&page);
		if (ret == EOK) {
			ret = _hal_addMemEntry(page.addr, SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE);
			if (ret == 0) {
				ptable = (addr_t *)(syspage->hs.ptable + VADDR_KERNEL);
				ptable[((u32)hal_config.ptable >> 12) & 0x000003ffU] = (page.addr & ~(SIZE_PAGE - 1U)) | (PGHD_WRITE | PGHD_PRESENT);
				hal_tlbInvalidateLocalEntry(NULL, hal_config.ptable);
				hal_memset(hal_config.ptable, 0, SIZE_PAGE);
				ret = _pmap_enter(pdir, hal_config.ptable, paddr, vaddr, attr, &page, 0);
			}
		}
	}
	return ret;
}


static void *_hal_configMapObject(u32 *pdir, addr_t start, void **vaddr, size_t size, vm_attr_t attr)
{
	void *result;
	addr_t end = start + size, paddr;
	u32 offset;
	int ret;

	if ((end & (SIZE_PAGE - 1U)) != 0U) {
		end += SIZE_PAGE;
		end &= ~(SIZE_PAGE - 1U);
	}
	offset = start;
	start &= ~(SIZE_PAGE - 1U);
	offset -= start;
	result = *vaddr;
	for (paddr = start; paddr < end; paddr += SIZE_PAGE) {
		ret = _hal_configMapPage(pdir, paddr, *vaddr, attr);
		if (ret != EOK) {
			*vaddr = result;
			return NULL;
		}
		*vaddr += SIZE_PAGE;
	}
	return result + offset;
}


static void *_hal_configMapObjectBeforeStack(u32 *pdir, addr_t start, size_t size, vm_attr_t attr)
{
	return _hal_configMapObject(pdir, start, &hal_config.heapStart, size, attr);
}


void *_hal_configMapDevice(u32 *pdir, addr_t start, size_t size, vm_attr_t attr)
{
	return _hal_configMapObject(pdir, start, &hal_config.devices, size, attr | PGHD_DEV);
}


static int _hal_acpiInit(hal_config_t *config)
{
	addr_t *pdir = (addr_t *)(VADDR_KERNEL + syspage->hs.pdir);

	if (syspage->hs.acpi_version != ACPI_NONE) {
		if (syspage->hs.madt != 0U) {
			hal_config.madt = _hal_configMapObjectBeforeStack(pdir, syspage->hs.madt, syspage->hs.madtLength, PGHD_WRITE);
		}
		if (syspage->hs.fadt != 0U) {
			hal_config.fadt = _hal_configMapObjectBeforeStack(pdir, syspage->hs.fadt, syspage->hs.fadtLength, PGHD_WRITE);
		}
		if (syspage->hs.hpet != 0U) {
			hal_config.hpet = _hal_configMapObjectBeforeStack(pdir, syspage->hs.hpet, syspage->hs.hpetLength, PGHD_WRITE);
		}

		if (hal_config.madt != NULL) {
			config->localApicAddr = _hal_configMapDevice(pdir, hal_config.madt->localApicAddr, SIZE_PAGE, PGHD_WRITE);
		}
		config->acpi = syspage->hs.acpi_version;
		return EOK;
	}
	else {
		return -EFAULT;
	}
}


static inline void _hal_configMemoryInit(void)
{
	const syspage_map_t *map;

	/* BIOS Data Area */
	(void)_hal_addMemEntry(0U, SIZE_PAGE, PAGE_OWNER_KERNEL);
	/* Add GDT and IDT to memory map. Note: according to IA32 specification, size of gdtr and idtr is 1 more
	   than the value we extract from syspage. */
	(void)_hal_addMemEntry(syspage->hs.gdtr.addr - VADDR_KERNEL, (u32)syspage->hs.gdtr.size + 1U, PAGE_OWNER_KERNEL | PAGE_KERNEL_CPU);
	(void)_hal_addMemEntry(syspage->hs.idtr.addr - VADDR_KERNEL, (u32)syspage->hs.idtr.size + 1U, PAGE_OWNER_KERNEL | PAGE_KERNEL_CPU);
	/* Add stack, page directory, page table, ebda and kernel to memory map */
	(void)_hal_addMemEntry(syspage->hs.pdir, SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE);
	(void)_hal_addMemEntry(syspage->hs.ptable, SIZE_PAGE, PAGE_OWNER_KERNEL | PAGE_KERNEL_PTABLE);
	(void)_hal_addMemEntry(syspage->hs.stack - syspage->hs.stacksz, syspage->hs.stacksz, PAGE_OWNER_KERNEL | PAGE_KERNEL_STACK);
	/* Add syspage to the memory map */
	(void)_hal_addMemEntry((addr_t)syspage - VADDR_KERNEL, sizeof(*syspage), PAGE_OWNER_KERNEL | PAGE_KERNEL_SYSPAGE);
	(void)_hal_addMemEntry(hal_config.ebda, 32U * SIZE_PAGE, PAGE_OWNER_BOOT);
	(void)_hal_addMemEntry(syspage->pkernel, (ptr_t)&_end - (ptr_t)(VADDR_KERNEL + syspage->pkernel), PAGE_OWNER_KERNEL);

	/* Calculate physical address space range */
	hal_config.minAddr = 0xffffffffU;
	hal_config.maxAddr = 0x00000000U;

	map = syspage->maps;
	if (map == NULL) {
		return;
	}

	do {
		if (map->start < hal_config.minAddr) {
			hal_config.minAddr = map->start;
		}

		if (map->end > hal_config.maxAddr) {
			hal_config.maxAddr = map->end;
		}
		map = map->next;
	} while (map != syspage->maps);

	hal_config.heapStart = (void *)(((ptr_t)&_end + SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U));
	if (hal_config.heapStart < (void *)(VADDR_KERNEL + 0xa0000U)) {
		hal_config.heapStart = (void *)(VADDR_KERNEL + 0x00100000U);
	}
	/* Initialize temporary page table (used for page table mapping) */
	hal_config.ptable = hal_config.heapStart;
	hal_config.heapStart += SIZE_PAGE;
}


void _hal_gasAllocDevice(const hal_gas_t *gas, hal_gasMapped_t *mgas, size_t size)
{
	addr_t *pdir = (addr_t *)(VADDR_KERNEL + syspage->hs.pdir);
	mgas->addressSpaceId = gas->addressSpaceId;
	mgas->registerWidth = gas->registerWidth;
	mgas->registerOffset = gas->registerOffset;
	mgas->accessSize = gas->accessSize;

	switch (gas->addressSpaceId) {
		case GAS_ADDRESS_SPACE_ID_MEMORY:
			mgas->address = _hal_configMapDevice(pdir, (addr_t)gas->address, size, PGHD_WRITE);
			break;
		default:
			mgas->address = (void *)((u32)gas->address);
			break;
	}
}


int _hal_gasWrite32(hal_gasMapped_t *gas, u32 offset, u32 val)
{
	int ret;
	switch (gas->addressSpaceId) {
		case GAS_ADDRESS_SPACE_ID_MEMORY:
			*(volatile u32 *)(gas->address + offset) = val;
			ret = 0;
			break;
		case GAS_ADDRESS_SPACE_ID_IOPORT:
			hal_outl((u16)(addr_t)(gas->address + offset), val);
			ret = 0;
			break;
		case GAS_ADDRESS_SPACE_ID_PCI:
			/* TODO */
			ret = 1;
			break;
		case GAS_ADDRESS_SPACE_ID_PCIBAR:
			/* TODO */
			ret = 1;
			break;
		default:
			/* Unspecified */
			ret = 1;
			break;
	}
	return ret;
}


int _hal_gasRead32(hal_gasMapped_t *gas, u32 offset, u32 *val)
{
	int ret;
	switch (gas->addressSpaceId) {
		case GAS_ADDRESS_SPACE_ID_MEMORY:
			*val = *(volatile u32 *)(gas->address + offset);
			ret = 0;
			break;
		case GAS_ADDRESS_SPACE_ID_IOPORT:
			*val = hal_inl((u16)(addr_t)(gas->address + offset));
			ret = 0;
			break;
		case GAS_ADDRESS_SPACE_ID_PCI:
			/* TODO */
			ret = 1;
			break;
		case GAS_ADDRESS_SPACE_ID_PCIBAR:
			/* TODO */
			ret = 1;
			break;
		default:
			/* Unspecified */
			ret = 1;
			break;
	}
	return ret;
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
void _hal_configInit(syspage_t *s)
{
	unsigned int ra, rb, rc, rd;
	u32 *pdir;

	hal_config.localApicAddr = NULL;
	hal_config.acpi = ACPI_NONE;
	hal_config.ebda = s->hs.ebda;
	hal_config.flags = 0U;
	hal_config.minAddr = 0U;
	hal_config.maxAddr = 0U;
	hal_config.heapStart = NULL;
	hal_config.ptable = NULL;
	hal_config.madt = NULL;
	hal_config.fadt = NULL;
	hal_config.hpet = NULL;
	hal_config.devices = MMIO_DEVICES_VIRT_ADDR;
	hal_config.memMap.count = 0U;

	init_common.pageIterator = 0U;

	/* Relocate syspage */
	syspage = (void *)s + VADDR_KERNEL;

	/* Relocate GDT and IDT */
	syspage->hs.gdtr.addr += VADDR_KERNEL;
	syspage->hs.idtr.addr += VADDR_KERNEL;
	/* clang-format off */
	__asm__ volatile (
		"lgdt %0\n\t"
		"lidt %1\n\t"
	:
	: "m" (syspage->hs.gdtr), "m" (syspage->hs.idtr)
	:);
	/* clang-format on */


	/* Obtain some CPU configuration with cpuid */
	hal_cpuid(1U, 0U, &ra, &rb, &rc, &rd);

	_hal_configMemoryInit();

	if ((_hal_acpiInit(&hal_config) != EOK) || (hal_config.madt == NULL)) {
		/* TODO: Try to find and load MP tables */
	}

	if (hal_isLapicPresent() == 0) {
		pdir = (u32 *)syspage->hs.pdir;
		/* Check presence of APIC with CPUID */
		if ((rd & 0x200U) != 0U) {
			hal_config.localApicAddr = _hal_configMapDevice(pdir, LAPIC_DEFAULT_ADDRESS, SIZE_PAGE, PGHD_WRITE);
		}
	}
}
