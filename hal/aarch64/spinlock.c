/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2024 Phoenix Systems
 * Author: Jacek Maksymowicz
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
	__asm__ volatile (
		"mrs x2, daif\n"
		"msr daifSet, #3\n"
		"str w2, [%0]\n"
		"b 2f\n"
	"1:\n"
		"wfe\n"
	"2:\n"
		"ldaxrb w2, [%1]\n"
		"cbz w2, 1b\n"
		"stxrb w2, wzr, [%1]\n"
		"cbnz w2, 2b\n"
	:
	: "r" (sc), "r" (&spinlock->lock)
	: "x2", "memory");
	/* clang-format on */
}


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile (
		"mov w2, #1\n"
		"stlrb w2, [%0]\n" /* Global monitor clear generates an event, SEV not necessary */
		"ldr w2, [%1]\n"
		"msr daif, x2\n"
	:
	: "r" (&spinlock->lock), "r" (sc)
	: "x2", "memory");
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
