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

#ifndef _HAL_ARMV7A_EXCEPTIONS_H_
#define _HAL_ARMV7A_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT 128

#define EXC_UNDEFINED 1
#define EXC_PAGEFAULT 4

#define SIZE_CTXDUMP            512 /* Size of dumped context */
#define SIZE_COREDUMP_GREGSET   72
#define SIZE_COREDUMP_THREADAUX 280 /* vfp context note */
#define SIZE_COREDUMP_GENAUX    36  /* auxv HWCAP note */

#define HAL_ELF_MACHINE 40 /* ARM */


typedef struct _exc_context_t {
	u32 dfsr;
	u32 dfar;
	u32 ifsr;
	u32 ifar;

	cpu_context_t cpuCtx;
} exc_context_t;

#endif
