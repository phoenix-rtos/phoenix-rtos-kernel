
/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Multiboot support
 *
 * Copyright 2017 Phoenix Systems
 * Author: Michał Mirosław, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "syspage.h"
#include "multiboot.h"
#include "cpu.h"
#include "pmap.h"
#include "string.h"
#include "../../proc/elf.h"

#include "../../include/errno.h"


extern void _start(void);
extern void _end(void);


__attribute__((aligned(SIZE_PAGE)))
struct {
	u8 stack[SIZE_PAGE];
	u8 syspage[SIZE_PAGE];
	u8 pdir[SIZE_PAGE];
	u8 ptable[SIZE_PAGE];
	u8 gdt[SIZE_PAGE / 2];
	u8 idt[SIZE_PAGE / 2];
} multiboot_common;


#if 0
static void multiboot_print(const char *s)
{
	for (; *s != 0; s++) {

		/* Wait for transmitter readiness */
		while (!(hal_inb((void *)0x3f8 + 5) & 0x20));

		hal_outb((void *)0x3f8, *s);
	}
}
#endif


void *_multiboot_init(multiboot_info_t *mbi)
{
	syspage_t *relsyspage;
	u32 v;
	u32 *dt;
	multiboot_mmitem_t *mi;
	syspage_mmitem_t *si;
	unsigned int i, k;
	syspage_program_t *p;
	multiboot_mod_t *mod;

	relsyspage = (void *)&multiboot_common.syspage - VADDR_KERNEL;

	/* Initialize GDT */
	v = SIZE_PAGE / 2;
	hal_memcpy(relsyspage->gdtr, &v, 2);
	v = (u32)multiboot_common.gdt - VADDR_KERNEL;
	hal_memcpy(&relsyspage->gdtr[2], &v, 4);

	dt = (u32 *)v;
	dt[0] = 0;
	dt[1] = 0;

	/* ring 0 code segment */
	dt[2] = 0x0000ffff;
	dt[3] = 0x00cf9a00;

	/* ring 0 data segment */
	dt[4] = 0x0000ffff;
	dt[5] = 0x00cf9200;

	/* Initialize IDT */
	v = SIZE_PAGE / 2;
	hal_memcpy(relsyspage->idtr, &v, 2);
	v = (u32)multiboot_common.idt - VADDR_KERNEL;
	hal_memcpy(&relsyspage->idtr[2], &v, 4);

	/* Initialize page dir and table addresses */
	relsyspage->pdir = (u32)multiboot_common.pdir - VADDR_KERNEL;

hal_memset((void *)relsyspage->pdir, 0, SIZE_PAGE);

	relsyspage->ptable = (u32)multiboot_common.ptable - VADDR_KERNEL;

	/* Initialize kernel data */
	relsyspage->stack = (u32)multiboot_common.stack - VADDR_KERNEL + SIZE_PAGE;
	relsyspage->stacksize = (u32)SIZE_PAGE;
	relsyspage->kernel = (u32)_start - VADDR_KERNEL;
	relsyspage->kernelsize  = (u32)_end - (u32)_start;
	relsyspage->console = 0;

	if (mbi->flags & MULTIBOOT_INFO_CMDLINE) {
		hal_strncpy(relsyspage->arg, (void *)mbi->cmdline, sizeof(relsyspage->arg) - 1);
		relsyspage->arg[sizeof(relsyspage->arg)-1] = 0;
	}

	/* Initialize memory map based on multiboot info */
	if (mbi->flags & MULTIBOOT_INFO_MEMMAP) {

		mi = (multiboot_mmitem_t *)mbi->mmap_addr;
		for (i = 0; (mi < (multiboot_mmitem_t *)(mbi->mmap_addr + mbi->mmap_length)) && (i < 64); i++) {
			si = &relsyspage->mm[i];
			si->addr = mi->addr;
			si->len = mi->len;
			si->attr = 0;

			if (mi->type == memAvail) {
				si->attr = 1;

			}
			mi = (void *)mi + mi->size + 4;
		}
		relsyspage->mmsize = i;
	}
	else if (mbi->flags & MULTIBOOT_INFO_MEMORY) {
		relsyspage->mmsize = mbi->mem_upper ? 2 : 1;

		relsyspage->mm[0].addr = 0;
		relsyspage->mm[0].len = mbi->mem_lower * 1024;
		relsyspage->mm[0].attr = 1;

		relsyspage->mm[1].addr = 0x100000;
		relsyspage->mm[1].len = mbi->mem_upper * 1024 - 0x100000;
		relsyspage->mm[1].attr = 1;
	}
	else
		return NULL;

	/* Copy programs loaded into memory */
	if (mbi->flags & MULTIBOOT_INFO_MODS) {
		p = relsyspage->progs;
		mod = (multiboot_mod_t *)mbi->mods_addr;

		for (i = 0; i < mbi->mods_count; i++, mod++, p++) {
			p->start = mod->mod_start;
			p->end = mod->mod_end;

			k = hal_strlen((char *)mod->cmdline);
			while (--k && ((char *)mod->cmdline)[k - 1] != '/');

			hal_memset(p->cmdline, 0, sizeof(p->cmdline));
			hal_strncpy(p->cmdline, (char *)mod->cmdline + k, sizeof(p->cmdline) - 1);
		}

		relsyspage->progssz = i;
	}
	else
		relsyspage->progssz = 0;

	return relsyspage;
}
