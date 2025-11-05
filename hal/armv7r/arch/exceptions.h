/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling (ARMv7r)
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_ARMV7R_EXCEPTIONS_H_
#define _PH_HAL_ARMV7R_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT 128U

#define EXC_UNDEFINED 3U

#define SIZE_CTXDUMP 512U /* Size of dumped context */

typedef struct _exc_context_t {
	u32 dfsr;
	u32 dfar;
	u32 ifsr;
	u32 ifar;

	cpu_context_t cpuCtx;
} exc_context_t;

#endif
