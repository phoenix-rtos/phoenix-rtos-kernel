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


#define EXC_DEFAULT   128
#define EXC_UNDEFINED 2
#define EXC_PAGEFAULT 127

#define SIZE_CTXDUMP 1024 /* Size of dumped context */

typedef cpu_context_t exc_context_t;

#endif
