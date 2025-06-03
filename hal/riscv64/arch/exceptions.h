/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_RISCV64_EXCEPTIONS_H_
#define _HAL_RISCV64_EXCEPTIONS_H_

#include "cpu.h"


#define EXC_DEFAULT   128
#define EXC_UNDEFINED 2
#define EXC_PAGEFAULT 127

#define SIZE_CTXDUMP          1024 /* Size of dumped context */
#define SIZE_COREDUMP_GREGSET 256
#ifdef PROC_COREDUMP_FPUCTX
#define SIZE_COREDUMP_THREADAUX 284
#else
#define SIZE_COREDUMP_THREADAUX 0
#endif
#define SIZE_COREDUMP_GENAUX 0

#define HAL_ELF_MACHINE 243 /* RISC-V 64-bit */

typedef cpu_context_t exc_context_t;

#endif
