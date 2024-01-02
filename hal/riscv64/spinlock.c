/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2018, 2023 Phoenix Systems
 * Author: Pawel Pisarczyk, Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/spinlock.h"
#include "hal/cpu.h"
#include "hal/list.h"


static struct {
	spinlock_t spinlock;
	spinlock_t *first;
} spinlock_common;


static void _hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{
	spinlock->lock = 1;

	spinlock->name = name;

	HAL_LIST_ADD(&spinlock_common.first, spinlock);
}


void hal_spinlockSet(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile
	(" \
		csrrc t0, sstatus, 2; \
		sd t0, (%0); \
		mv t0, zero; \
	1: \
		amoswap.w.aq t0, t0, %1; \
		beqz t0, 1b"
	:
	: "r" (sc), "A" (spinlock->lock)
	: "t0", "memory");
	/* clang-format on */
}


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile
	(" \
		li t1, 1; \
		amoswap.w.rl t1, t1, %0; \
		ld t0, (%1); \
		csrw sstatus, t0"
	:
	: "A" (spinlock->lock), "r" (sc)
	: "t0", "t1", "memory");
	/* clang-format on */
}


void hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&spinlock_common.spinlock, &sc);
	_hal_spinlockCreate(spinlock, name);
	hal_spinlockClear(&spinlock_common.spinlock, &sc);
}


void hal_spinlockDestroy(spinlock_t *spinlock)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&spinlock_common.spinlock, &sc);

	HAL_LIST_REMOVE(&spinlock_common.first, spinlock);

	hal_spinlockClear(&spinlock_common.spinlock, &sc);
}


__attribute__((section(".init"))) void _hal_spinlockInit(void)
{
	spinlock_common.first = NULL;
	_hal_spinlockCreate(&spinlock_common.spinlock, "spinlock_common.spinlock");
}
