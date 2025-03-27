/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2014, 2017, 2024 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_AARCH64_SPINLOCK_H_
#define _HAL_AARCH64_SPINLOCK_H_

#include "hal/types.h"

typedef struct _spinlock_t {
	u8 lock;
	const char *name;
	struct _spinlock_t *next;
	struct _spinlock_t *prev;
} spinlock_t;


typedef u32 spinlock_ctx_t;


#endif
