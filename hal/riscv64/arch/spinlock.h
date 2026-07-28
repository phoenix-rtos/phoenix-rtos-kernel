/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2012, 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_RISCV64_SPINLOCK_H_
#define _PH_HAL_RISCV64_SPINLOCK_H_

#include "hal/types.h"


typedef u64 spinlock_ctx_t;

typedef struct _spinlock_t {
	const char *name;
	struct _spinlock_t *next;
	struct _spinlock_t *prev;

	u32 lock;
} __attribute__((packed, aligned(8))) spinlock_t;


#endif
