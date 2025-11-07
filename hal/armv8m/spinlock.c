/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2017, 2022, 2023 Phoenix Systems
 * Author: Pawel Pisarczyk, Damian Loewnau, Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/spinlock.h"
#include "hal/list.h"

static struct {
	spinlock_t spinlock;
	spinlock_t *first;
} spinlock_common;


void hal_spinlockSet(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile(" \
		mrs r2, primask; \
		cpsid i; \
		str r2, [%0]; \
		mov r3, #0; \
	1: \
		ldrexb r2, [%1]; \
		cmp r2, #0; \
		beq 1b; \
		strexb r2, r3, [%1]; \
		cmp r2, #0; \
		bne 1b; \
		dmb"
	:
	: "r" (sc), "r" (&spinlock->lock)
	: "r2", "r3", "memory", "cc");
	/* clang-format on */
}


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile (" \
	1 : \
		ldrexb r2, [%0]; \
		add r2, r2, #1; \
		dmb; \
		strexb r3, r2, [%0]; \
		cmp r3, #0; \
		bne 1b; \
		ldr r2, [%1]; \
		msr primask, r2;"
	:
	: "r" (&spinlock->lock), "r" (sc)
	: "r2", "r3", "memory", "cc");
	/* clang-format on */
}


void _hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{
	spinlock->lock = 1U;
	spinlock->name = name;

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

	if (spinlock->next == spinlock) {
		spinlock_common.first = NULL;
	}
	else {
		spinlock->prev->next = spinlock->next;
		spinlock->next->prev = spinlock->prev;
	}
	spinlock->prev = spinlock->next = NULL;

	hal_spinlockClear(&spinlock_common.spinlock, &sc);
}


__attribute__((section(".init"))) void _hal_spinlockInit(void)
{
	spinlock_common.first = NULL;
	_hal_spinlockCreate(&spinlock_common.spinlock, "spinlock_common.spinlock");
}
