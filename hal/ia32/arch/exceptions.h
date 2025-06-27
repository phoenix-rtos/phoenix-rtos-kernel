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

#ifndef _HAL_IA32_EXCEPTIONS_H_
#define _HAL_IA32_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT 128

#define EXC_UNDEFINED 6
#define EXC_PAGEFAULT 14

#define SIZE_CTXDUMP          512 /* Size of dumped context */
#define SIZE_COREDUMP_GREGSET 68
#ifdef PROC_COREDUMP_FPUCTX
#define SIZE_COREDUMP_THREADAUX 128
#else
#define SIZE_COREDUMP_THREADAUX 0
#endif
#define SIZE_COREDUMP_GENAUX 0

#define HAL_ELF_MACHINE 3 /* IA32 */


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
