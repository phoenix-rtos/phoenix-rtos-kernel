/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2017, 2022 Phoenix Systems
 * Author: Pawel Pisarczyk, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/spinlock.h"

static struct {
	spinlock_t spinlock;
	spinlock_t *first;
} spinlocks;


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
	spinlock->lock = 1u;
	spinlock->name = name;

	if (spinlocks.first != NULL) {
		spinlocks.first->prev->next = spinlock;
		spinlock->prev = spinlocks.first->prev;
		spinlock->next = spinlocks.first;
		spinlocks.first->prev = spinlock;
	}
	else {
		spinlocks.first = spinlock;
		spinlock->next = spinlock;
		spinlock->prev = spinlock;
	}
	return;
}


void hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&spinlocks.spinlock, &sc);
	_hal_spinlockCreate(spinlock, name);
	hal_spinlockClear(&spinlocks.spinlock, &sc);
}


void hal_spinlockDestroy(spinlock_t *spinlock)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&spinlocks.spinlock, &sc);

	if (spinlock->next == spinlock) {
		spinlocks.first = NULL;
	}
	else {
		spinlock->prev->next = spinlock->next;
		spinlock->next->prev = spinlock->prev;
	}
	spinlock->prev = spinlock->next = NULL;

	hal_spinlockClear(&spinlocks.spinlock, &sc);
}


__attribute__((section(".init"))) void _hal_spinlockInit(void)
{
	spinlocks.first = NULL;
	_hal_spinlockCreate(&spinlocks.spinlock, "spinlocks.spinlock");
}
