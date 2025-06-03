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

#define SIZE_CTXDUMP          550
#define SIZE_COREDUMP_GREGSET 432
#ifdef PROC_COREDUMP_FPUCTX
#define SIZE_COREDUMP_THREADAUX 416
#else
#define SIZE_COREDUMP_THREADAUX 0
#endif
#define SIZE_COREDUMP_GENAUX 0

#define HAL_ELF_MACHINE 2 /* SPARC */

#pragma pack(push, 1)

typedef struct _exc_context_t {
	/* special */
	u32 wim;
	u32 tbr;

	cpu_context_t cpuCtx;
} exc_context_t;

#pragma pack(pop)


#endif
