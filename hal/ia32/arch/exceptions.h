/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2012 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_IA32_EXCEPTIONS_H_
#define _PH_HAL_IA32_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT 128

#define EXC_UNDEFINED 6
#define EXC_PAGEFAULT 14

/* Size of dumped context */
#ifndef NDEBUG
#define SIZE_CTXDUMP 1024
#else
#define SIZE_CTXDUMP 512
#endif


#pragma pack(push, 1)

typedef struct {
	u32 err;
	u32 dr0;
	u32 dr1;
	u32 dr2;
	u32 dr3;
	u32 dr6;
	u32 dr7;
	cpu_context_t cpuCtx;
} exc_context_t;

#pragma pack(pop)

#endif
