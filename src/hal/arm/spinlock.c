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

#include "spinlock.h"
#include "cpu.h"


struct {
	spinlock_t spinlock;
	spinlock_t *first;
} spinlocks;


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
	hal_spinlockSet(&spinlocks.spinlock);
	_hal_spinlockCreate(spinlock, name);
	hal_spinlockClear(&spinlocks.spinlock);
}


void hal_spinlockDestroy(spinlock_t *spinlock)
{
	hal_spinlockSet(&spinlocks.spinlock);

	if (spinlock->next == spinlock)
		spinlocks.first = NULL;
	else {
		spinlock->prev->next = spinlock->next;
		spinlock->next->prev = spinlock->prev;
	}
	spinlock->prev = spinlock->next = NULL;

	hal_spinlockClear(&spinlocks.spinlock);
}


__attribute__ ((section (".init"))) void _hal_spinlockInit(void)
{
	spinlocks.first = NULL;
	_hal_spinlockCreate(&spinlocks.spinlock, "spinlocks.spinlock");
}
