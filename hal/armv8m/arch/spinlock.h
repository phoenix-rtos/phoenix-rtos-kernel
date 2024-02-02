/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2014, 2017, 2022 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ARMV8M_SPINLOCK_H_
#define _HAL_ARMV8M_SPINLOCK_H_

#include "hal/types.h"

typedef struct _spinlock_t {
	const char *name;
	struct _spinlock_t *next;
	struct _spinlock_t *prev;

	u8 lock;
} spinlock_t;


typedef u32 spinlock_ctx_t;


#endif
