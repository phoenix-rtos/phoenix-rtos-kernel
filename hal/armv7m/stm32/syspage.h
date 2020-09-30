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
	u32 start;
	u32 end;
	int dmap;

	char cmdline[16];
} syspage_program_t;


typedef struct _syspage_t {
	u32 pbegin;
	u32 pend;

	char *arg;

	u32 progssz;
	syspage_program_t progs[0];
} syspage_t;

#pragma pack(pop)


/* Syspage */
extern syspage_t *syspage;


void _hal_syspageInit(void);


#endif

#endif
