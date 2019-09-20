/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Empty file holding programs
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include HAL
#include "vm/vm.h"
#include "programs.h"

#include "../include/errno.h"


#ifndef CPIO_PAD
#define CPIO_PAD 0x3
#endif

extern char programs[] __attribute__((align(4096)));

unsigned int programs_a2i(char *s)
{
	unsigned int i, k = 28, v = 0;
	char d;

	for (i = 0; (i < 8) && s[i]; i++) {
		d = s[i] - '0';

		if ((d > 16) && (d < 23))
			d -= 7;
		else if ((d > 48) && (d < 55))
			d -= 39;
		else if (d > 9)
			return -EINVAL;

		v += (d << k);
		k -= 4;
	}
	return v;	
}


int programs_decode(vm_map_t *kmap, vm_object_t *kernel)
{
#if !defined(CPU_STM32) && !defined(CPU_IMXRT)
	cpio_newc_t *cpio = (void *)&programs;
	unsigned int fs, ns, sz, k;
	page_t *p;
	void *vaddr;
	syspage_program_t *pr;

	if (hal_strncmp(cpio->c_magic, "070701", 6)) {
		/* Happens in QEMU when programs are not page aligned. What the hell? */
		cpio = (char *)&programs - 0x1000;

		if (hal_strncmp(cpio->c_magic, "070701", 6)) {
			lib_printf("valid cpio not found\n");
			return -EINVAL;
		}

		lib_printf("cpio found dislocated\n");
	}

	for (;;) {
		if (!hal_strcmp(cpio->name, "TRAILER!!!"))
			break;

		pr = &syspage->progs[syspage->progssz++];

		/* Initialize cmdline */
		k = hal_strlen((char *)cpio->name);
		while (--k && ((char *)cpio->name)[k - 1] != '/');

		hal_memset(pr->cmdline, 0, sizeof(pr->cmdline));
		hal_strncpy(pr->cmdline, (char *)cpio->name + k, sizeof(pr->cmdline) - 1);

		fs = programs_a2i(cpio->c_filesize);
		ns = programs_a2i(cpio->c_namesize);

		cpio = (void *)(((ptr_t)cpio + sizeof(cpio_newc_t) + ns + CPIO_PAD) & ~CPIO_PAD);

		/* Alloc pages for program */
		sz = ((fs + SIZE_PAGE - 1) / SIZE_PAGE) * SIZE_PAGE;
		if ((p = vm_pageAlloc(sz, PAGE_OWNER_APP)) == NULL)
			return -ENOMEM;

		if ((vaddr = vm_mmap(kmap, kmap->start, p, sz, PROT_READ | PROT_WRITE, kernel, -1, MAP_NONE)) == NULL) {
			vm_pageFree(p);
			return -ENOMEM;
		}

		hal_memcpy(vaddr, cpio, fs);
//		vm_munmap(kmap, vaddr, sz);

		pr->start = (typeof(pr->start))p->addr;
		pr->end = (typeof(pr->end))p->addr + fs;

		cpio = (void *)(((ptr_t)cpio + fs + CPIO_PAD) & ~CPIO_PAD);
	}
#endif

	return EOK;
}
