/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System information page (prepared by kernel loader)
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_SYSPAGE_H_
#define _HAL_SYSPAGE_H_

#include "cpu.h"


#ifndef __ASSEMBLY__

#pragma pack(push, 1)

typedef struct syspage_program_t {
	void *entry;
	u32 hdrssz;
	void *got;
	u32 gotsz;
	u32 offset;
	u32 size;
	char *cmdline;
	struct {
		u32 addr;
		u32 memsz;
		u32 flags;
		u32 vaddr;
		u32 filesz;
		u32 align;
	} hdrs[3];
} syspage_program_t;


typedef struct _syspage_t {
	char *arg;

	u32 progssz;
	syspage_program_t progs[0];
} syspage_t;

#pragma pack(pop)


/* Syspage */
extern syspage_t *syspage;

#endif

#endif
