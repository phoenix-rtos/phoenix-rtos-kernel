/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/spinlock.h"
#include "hal/cpu.h"

struct {
	spinlock_t spinlock;
	spinlock_t *first;
} spinlocks;


void hal_spinlockSet(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
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
}


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	__asm__ volatile (" \
	1 : \
		ldrexb r1, [%0]; \
		add r1, r1, #1; \
		dmb; \
		strexb r2, r1, [%0]; \
		cmp r2, #0; \
		bne 1b; \
		ldrb r1, [%1]; \
		msr cpsr_c, r1;"
	:
	: "r" (&spinlock->lock), "r" (sc)
	: "r1", "r2", "memory");
}


void _hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{
	spinlock->lock = 1;

	spinlock->name = name;
/*	spinlock->dmin = (cycles_t)-1;
	spinlock->dmax = (cycles_t)0; */

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

	if (spinlock->next == spinlock)
		spinlocks.first = NULL;
	else {
		spinlock->prev->next = spinlock->next;
		spinlock->next->prev = spinlock->prev;
	}
	spinlock->prev = spinlock->next = NULL;

	hal_spinlockClear(&spinlocks.spinlock, &sc);
}


__attribute__ ((section (".init"))) void _hal_spinlockInit(void)
{
	spinlocks.first = NULL;
	_hal_spinlockCreate(&spinlocks.spinlock, "spinlocks.spinlock");
}
