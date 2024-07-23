/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2014, 2017, 2024 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ARMV8R_SPINLOCK_H_
#define _HAL_ARMV8R_SPINLOCK_H_

#include "hal/types.h"

typedef struct _spinlock_t {
	const char *name;
	struct _spinlock_t *next;
	struct _spinlock_t *prev;

	u8 lock;
} __attribute__((packed)) spinlock_t;


typedef u32 spinlock_ctx_t;


#endif
