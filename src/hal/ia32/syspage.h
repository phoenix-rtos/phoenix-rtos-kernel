/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System information page (prepared by kernel loader)
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_SYSPAGE_H_
#define _HAL_SYSPAGE_H_

#include "cpu.h"


#define SIZE_SYSPAGE_MM   64


#ifndef __ASSEMBLY__

#pragma pack(push, 1)

typedef struct {
	u32 addr;
	u32 reserved0;
	u32 len;
	u32 reserved1;
	u16 attr;
	u16 reserved2;
} syspage_mmitem_t;


typedef struct syspage_program_t {
	u32 start;
	u32 end;

	char cmdline[16];
} syspage_program_t;


typedef struct {
	u16 limit;
	u32 addr;
	u16 pad;
} syspage_ia32_tr_t;


typedef struct _syspage_t {
	syspage_ia32_tr_t gdtr;
	syspage_ia32_tr_t idtr;

	u32 pdir;
	u32 ptable;

	u32 stack;
	u32 stacksize;

	u32 kernel;
	u32 kernelsize;
	u32 console;    /* com1, com2, com3, com4, galileo0, galileo1 */
	char arg[256];

	u16 mmsize;
	syspage_mmitem_t mm[SIZE_SYSPAGE_MM];

	u16 progssz;
	syspage_program_t progs[0];
} syspage_t;

#pragma pack(pop)


/* Syspage */
extern syspage_t *syspage;

#endif

#endif
