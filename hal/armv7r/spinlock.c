/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2017, 2023, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Hubert Badocha
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
	__asm__ volatile(
		"mrs r2, cpsr\n\t"
		"cpsid if\n\t"
		"strb r2, [%0]\n\t"
		"mov r3, #0\n"
	"1:\n\t"
		"ldrexb r2, [%1]\n\t"
		"cmp r2, #0\n\t"
		"beq 1b\n\t"
		"strexb r2, r3, [%1]\n\t"
		"cmp r2, #0\n\t"
		"bne 1b\n\t"
		"dmb"
	:
	: "r" (sc), "r" (&spinlock->lock)
	: "r2", "r3", "memory", "cc");
	/* clang-format on */
}


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile (
		"dmb\n"
	"1:\n\t"
		"ldrexb r2, [%0]\n\t"
		"add r2, r2, #1\n\t"
		"strexb r3, r2, [%0]\n\t"
		"cmp r3, #0\n\t"
		"bne 1b\n\t"
		"ldrb r2, [%1]\n\t"
		"msr cpsr_c, r2"
	:
	: "r" (&spinlock->lock), "r" (sc)
	: "r2", "r3", "memory");
	/* clang-format on */
}


void _hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{
	spinlock->lock = 1;

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

	HAL_LIST_REMOVE(&spinlock_common.first, spinlock);

	hal_spinlockClear(&spinlock_common.spinlock, &sc);
}


__attribute__((section(".init"))) void _hal_spinlockInit(void)
{
	spinlock_common.first = NULL;
	_hal_spinlockCreate(&spinlock_common.spinlock, "spinlock_common.spinlock");
}
