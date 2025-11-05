/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_ARMV7A_EXCEPTIONS_H_
#define _PH_HAL_ARMV7A_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT 128

#define EXC_UNDEFINED 1
#define EXC_PAGEFAULT 4

#define SIZE_CTXDUMP 512 /* Size of dumped context */


typedef struct _exc_context_t {
	u32 dfsr;
	u32 dfar;
	u32 ifsr;
	u32 ifar;

	cpu_context_t cpuCtx;
} exc_context_t;

#endif
