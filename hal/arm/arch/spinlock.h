/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock data structure definition
 *
 * Copyright 2014, 2017, 2022, 2026 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Damian Loewnau, Jacek Maksymowicz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_ARM_SPINLOCK_H_
#define _PH_HAL_ARM_SPINLOCK_H_

#include "hal/types.h"

typedef struct _spinlock_t {
	u8 lock; /* Must be first in the structure because it is used in platform-specific assembly code */
	const char *name;
	struct _spinlock_t *next;
	struct _spinlock_t *prev;
} spinlock_t;


typedef u32 spinlock_ctx_t;


#endif
