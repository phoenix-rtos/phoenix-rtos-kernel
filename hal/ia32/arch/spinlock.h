/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2012 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_IA32_SPINLOCK_H_
#define _HAL_IA32_SPINLOCK_H_

#include "hal/types.h"

typedef u32 spinlock_ctx_t;

typedef struct _spinlock_t {
	const char *name;
	cycles_t b;
	cycles_t e;
	cycles_t dmin;
	cycles_t dmax;
	struct _spinlock_t *next;
	struct _spinlock_t *prev;

	u32 lock;
} spinlock_t;

#endif
