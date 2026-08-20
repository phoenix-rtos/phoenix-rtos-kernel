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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_ARMV7A_EXCEPTIONS_H_
#define _PH_HAL_ARMV7A_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT 128U

#define EXC_UNDEFINED 1U
#define EXC_PAGEFAULT 4U

#define SIZE_CTXDUMP 512U /* Size of dumped context */

#define EXCJMP_SUPPORTED 1


typedef struct _exc_context_t {
	u32 dfsr;
	u32 dfar;
	u32 ifsr;
	u32 ifar;

	cpu_context_t cpuCtx;
} exc_context_t;


typedef struct _excjmp_context_t {
	u32 psr;
	u32 ret;
	u32 r4;
	u32 r5;
	u32 r6;
	u32 r7;
	u32 r8;
	u32 r9;
	u32 r10;
	u32 fp;
	u32 sp;
} excjmp_context_t;

#endif
