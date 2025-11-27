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

#ifndef _PH_HAL_RISCV64_EXCEPTIONS_H_
#define _PH_HAL_RISCV64_EXCEPTIONS_H_

#include "cpu.h"


#define EXC_DEFAULT   128U
#define EXC_UNDEFINED 2U
#define EXC_PAGEFAULT 127U

#define SIZE_CTXDUMP 1024U /* Size of dumped context */

typedef cpu_context_t exc_context_t;

#endif
