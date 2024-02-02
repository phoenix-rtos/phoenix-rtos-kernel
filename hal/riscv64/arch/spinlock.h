/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2012, 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_RISCV64_SPINLOCK_H_
#define _HAL_RISCV64_SPINLOCK_H_

#include "hal/types.h"


typedef u64 spinlock_ctx_t;

typedef struct _spinlock_t {
	const char *name;
	struct _spinlock_t *next;
	struct _spinlock_t *prev;

	u64 lock;
} spinlock_t;


#endif
