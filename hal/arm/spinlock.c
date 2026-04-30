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

#include "hal/spinlock.h"
#include "hal/cpu.h"
#include "hal/list.h"

#ifndef KERNEL_SPINLOCK_ARM_M
#error "KERNEL_SPINLOCK_ARM_M must be defined (1 for Cortex-M CPU, 0 for other ARM CPU)"
#endif


static struct {
	spinlock_t spinlock;
	spinlock_t *first;
} spinlock_common;


void hal_spinlockSet(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile(
#if KERNEL_SPINLOCK_ARM_M
		"mrs r2, primask\n"
		"cpsid i\n"
#else
		"mrs r2, cpsr\n"
		"cpsid if\n"
#endif
		"str r2, [%0]\n"
		"mov %0, #0\n"
	"1:\n"
		"ldrexb r2, [%1]\n"
		"cmp r2, #0\n"
		"beq 1b\n"
		"strexb r2, %0, [%1]\n"
		"cmp r2, #0\n"
		"bne 1b\n"
		"dmb\n"
	: "+r" (sc)
	: "r" (&spinlock->lock)
	: "r2", "memory", "cc");
	/* clang-format on */
}


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */
	__asm__ volatile (
		"mov r2, #1\n"
		"dmb\n"
		"strb r2, [%0]\n"
		"ldr r2, [%1]\n"
#if KERNEL_SPINLOCK_ARM_M
		"msr primask, r2\n"
#else
		"msr cpsr_c, r2\n"
#endif
	:
	: "r" (&spinlock->lock), "r" (sc)
	: "r2", "memory");
	/* clang-format on */
}


static void _hal_spinlockCreate(spinlock_t *spinlock, const char *name)
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
