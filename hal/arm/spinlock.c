/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock implementation for ARM CPUs
 *
 * Copyright 2017, 2023, 2024, 2026 Phoenix Systems
 * Author: Pawel Pisarczyk, Damian Loewnau, Hubert Badocha, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/cpu.h"
#include "hal/list.h"
#include "lib/lib.h"
#include "config.h"

#ifndef KERNEL_SPINLOCK_ARM_M
#error "KERNEL_SPINLOCK_ARM_M must be defined (1 for Cortex-M CPU, 0 for other ARM CPU)"
#endif


_Static_assert(offsetof(spinlock_t, lock) == 0U, "lock field must be first in the structure");

static struct {
	spinlock_t spinlock;
	spinlock_t *first;
} spinlock_common;


void hal_spinlockSet(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	u32 tmp;
	/* clang-format off */
	__asm__ volatile (
#if KERNEL_SPINLOCK_ARM_M
		"mrs %[tmp], primask\n"
		"cpsid i\n"
#else
		"mrs %[tmp], cpsr\n"
		"cpsid if\n"
#endif
	: [tmp] "=r" (tmp));
	*sc = tmp;

	__asm__ volatile (
	"1:\n"
		"ldrexb %[tmp], [%[lock]]\n"
		"cmp %[tmp], #0\n"
		"beq 1b\n"
		"strexb %[tmp], %[zero], [%[lock]]\n"
		"cmp %[tmp], #0\n"
		"bne 1b\n"
		"dmb\n"
	: [tmp] "=&r" (tmp) /* `tmp` cannot share register with `zero` or `lock` */
	: [lock] "r" (&spinlock->lock), [zero] "r" (0)
	: "memory", "cc");
	/* clang-format on */
}


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile (
		"dmb\n"
		"strb %[one], [%[lock]]\n"
	:
	: [one] "r" (1), [lock] "r" (&spinlock->lock)
	: "memory");

	__asm__ volatile (
#if KERNEL_SPINLOCK_ARM_M
		"msr primask, %[tmp]\n"
#else
		"msr cpsr_c, %[tmp]\n"
#endif
	:
	: [tmp] "r" (*sc));
	/* clang-format on */
}


static void _hal_spinlockCreate(spinlock_t *spinlock, const char *name)
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

	HAL_LIST_REMOVE(&spinlock_common.first, spinlock);

	hal_spinlockClear(&spinlock_common.spinlock, &sc);
}


__attribute__((section(".init"))) void _hal_spinlockInit(void)
{
	spinlock_common.first = NULL;
	_hal_spinlockCreate(&spinlock_common.spinlock, "spinlock_common.spinlock");
}
