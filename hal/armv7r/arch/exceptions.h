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

#ifndef _HAL_ARMV7R_EXCEPTIONS_H_
#define _HAL_ARMV7R_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT 128

#define EXC_UNDEFINED 3

#define SIZE_CTXDUMP 512 /* Size of dumped context */

typedef struct _exc_context_t {
	u32 dfsr;
	u32 dfar;
	u32 ifsr;
	u32 ifar;

	cpu_context_t cpuCtx;
} exc_context_t;

#endif
