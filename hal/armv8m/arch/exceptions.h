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

#define SIZE_CTXDUMP 512 /* Size of dumped context */


typedef cpu_context_t exc_context_t;

#endif
