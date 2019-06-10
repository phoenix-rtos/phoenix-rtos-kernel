/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System information page (prepared by kernel loader)
 *
 * Copyright 2017, 2019 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "syspage.h"
#include "string.h"

#define MAX_PROGSZ 16

u8 _syspage_store[sizeof(syspage_t) + MAX_PROGSZ * sizeof(syspage_program_t)];


syspage_t *syspage;


void _hal_syspageInit(void)
{
	u32 progsz = syspage->progssz;

	if (progsz > MAX_PROGSZ)
		progsz = MAX_PROGSZ;

	hal_memcpy(_syspage_store, syspage, sizeof(syspage_t) + progsz * sizeof(syspage_program_t));

	syspage = (void *)_syspage_store;
	syspage->progssz = progsz;
}
