/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * pmap - machine dependent part of VM subsystem (ARMv8r)
 *
 * Copyright 2017, 2020-2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Hubert Buczynski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch/cpu.h>
#include <config.h>

#include "hal/pmap.h"

/* parasoft-begin-suppress MISRAC2012-RULE_8_6 "Provided by toolchain" */
/* Linker symbols */
extern unsigned int _end;
extern unsigned int __bss_start;
/* parasoft-end-suppress MISRAC2012-RULE_8_6 */

/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Global variable used in assembler code" */
u8 _init_stack[NUM_CPUS][SIZE_INITIAL_KSTACK] __attribute__((aligned(8)));


/* Function creates empty page table */
int pmap_create(pmap_t *pmap, pmap_t *kpmap, page_t *p, void *vaddr)
{
	return 0;
}


addr_t pmap_destroy(pmap_t *pmap, int *i)
{
	return 0;
}


int pmap_addMap(pmap_t *pmap, unsigned int map)
{
	return 0;
}


int pmap_isAllowed(pmap_t *pmap, const void *vaddr, size_t size)
{
	/* TODO: MPU support */
	return 1;
}


void pmap_switch(pmap_t *pmap)
{
}


int pmap_enter(pmap_t *pmap, addr_t pa, void *vaddr, vm_attr_t attr, page_t *alloc)
{
	return 0;
}


int pmap_remove(pmap_t *pmap, void *vstart, void *vend)
{
	return 0;
}


addr_t pmap_resolve(pmap_t *pmap, void *vaddr)
{
	return (addr_t)vaddr;
}


int pmap_getPage(page_t *page, addr_t *addr)
{
	return 0;
}


char pmap_marker(page_t *p)
{
	return '\0';
}


int _pmap_kernelSpaceExpand(pmap_t *pmap, void **start, void *end, page_t *dp)
{
	return 0;
}


int pmap_segment(unsigned int i, void **vaddr, size_t *size, vm_prot_t *prot, void **top)
{
	if (i != 0U) {
		return -1;
	}

	/* Returns region above basic kernel's .bss section */
	*vaddr = (void *)&_end;
	*size = (((size_t)(*top) + SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U)) - (size_t)&_end;

	return 0;
}


void _pmap_init(pmap_t *pmap, void **vstart, void **vend)
{
	(*vstart) = (void *)(((ptr_t)&_end + 7U) & ~7U);
	(*vend) = (*((char **)vstart)) + SIZE_PAGE;

	pmap->start = (void *)&__bss_start;

	/* Initial size of kernel map */
	pmap->end = (void *)((addr_t)&__bss_start + 32U * 1024U);
}
