/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_LEON3_SPINLOCK_H_
#define _PH_HAL_LEON3_SPINLOCK_H_


#include "hal/types.h"


typedef u32 spinlock_ctx_t;


typedef struct _spinlock_t {
	const char *name;
	struct _spinlock_t *next;
	struct _spinlock_t *prev;
	u8 lock;
} __attribute__((packed)) spinlock_t;


#endif
