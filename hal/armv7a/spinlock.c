/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2017, 2023 Phoenix Systems
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


void hal_spinlockSet(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile(" \
		mrs r1, cpsr; \
		cpsid if; \
		strb r1, [%0]; \
		mov r2, #0; \
	1: \
		ldrexb r1, [%1]; \
		cmp r1, #0; \
		beq 1b; \
		strexb r1, r2, [%1]; \
		cmp r1, #0; \
		bne 1b; \
		dmb"
	:
	: "r" (sc), "r" (&spinlock->lock)
	: "r1", "r2", "memory", "cc");
	/* clang-format on */
}


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile (" \
		dmb; \
	1 : \
		ldrexb r1, [%0]; \
		add r1, r1, #1; \
		strexb r2, r1, [%0]; \
		cmp r2, #0; \
		bne 1b; \
		ldrb r1, [%1]; \
		msr cpsr_c, r1"
	:
	: "r" (&spinlock->lock), "r" (sc)
	: "r1", "r2", "memory");
	/* clang-format on */
}


static void _hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{
	spinlock->lock = 1;

	spinlock->name = name;
/*	spinlock->dmin = (cycles_t)-1;
	spinlock->dmax = (cycles_t)0; */

	HAL_LIST_ADD(&spinlock_common.first, spinlock);
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


__attribute__ ((section (".init"))) void _hal_spinlockInit(void)
{
	spinlock_common.first = NULL;
	_hal_spinlockCreate(&spinlock_common.spinlock, "spinlock_common.spinlock");
}
