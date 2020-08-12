/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System information page (prepared by kernel loader)
 *
 * Copyright 2014, 2018 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_SYSPAGE_H_
#define _HAL_SYSPAGE_H_

#include "cpu.h"


#ifndef __ASSEMBLY__

/* This structures are used in asm code in _init-imx6ull.S */

typedef struct syspage_program_t {
	u32 start;
	u32 end;

	char cmdline[16];
} syspage_program_t;


typedef struct _syspage_t {
	u32 pbegin;
	u32 pend;

	u32 kernel;
	u32 kernelsize;

	u32 console;    /* UART1, UART2, UART3, .. */
	char arg[256];

	u32 progssz;
	syspage_program_t progs[0];
} syspage_t;


/* Syspage */
extern syspage_t * const syspage;

#endif

#endif
