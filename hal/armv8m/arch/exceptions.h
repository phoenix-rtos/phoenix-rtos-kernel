/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exceptions handling
 *
 * Copyright 2022 Phoenix Systems
 * Author: Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ARMV8M_EXCEPTIONS_H_
#define _HAL_ARMV8M_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT 128

#define EXC_UNDEFINED 3

#define SIZE_CTXDUMP            512 /* Size of dumped context */
#define SIZE_COREDUMP_GREGSET   72
#define SIZE_COREDUMP_THREADAUX 0
#define SIZE_COREDUMP_GENAUX    0

#define HAL_ELF_MACHINE 40 /* ARM */

typedef cpu_context_t exc_context_t;

#endif
