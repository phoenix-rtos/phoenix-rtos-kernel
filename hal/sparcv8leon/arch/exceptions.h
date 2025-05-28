/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_LEON3_EXCEPTIONS_H_
#define _HAL_LEON3_EXCEPTIONS_H_

#include <arch/cpu.h>

#define EXC_DEFAULT 128

#define EXC_UNDEFINED 2

#ifndef NOMMU
#define EXC_PAGEFAULT 1
#define EXC_PAGEFAULT_DATA 9
#endif

#define SIZE_CTXDUMP 550

#pragma pack(push, 1)

typedef struct _exc_context_t {
	/* special */
	u32 wim;
	u32 tbr;

	cpu_context_t cpuCtx;
} exc_context_t;

#pragma pack(pop)


#endif
