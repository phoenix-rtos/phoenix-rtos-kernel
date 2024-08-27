/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2022, 2023 Phoenix Systems
 * Author: Lukasz Leczkowski, Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch/cpu.h>
#include "hal/spinlock.h"
#include "hal/list.h"

#define STR(x)  #x
#define XSTR(x) STR(x)


struct {
	spinlock_t spinlock;
	spinlock_t *first;
} spinlock_common;


void hal_spinlockSet(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */

	__asm__ volatile(" \
		rd %%psr, %%g2; \
		st %%g2, [%1]; \
		or %%g2, " XSTR(PSR_PIL) ", %%g2; \
		wr %%g2, %%psr; \
		nop; \
		nop; \
		nop; \
	.align 16; /* GRLIB TN-0011 errata */ \
	1: \
		ldstub [%0], %%g2; \
		tst %%g2; \
		be 3f; \
		nop; \
	2: \
		ldub [%0], %%g2; \
		tst %%g2; \
		bne 2b; \
		nop; \
		ba,a 1b; \
	3: \
		nop; \
	"
	:
	: "r"(&spinlock->lock), "r"(sc)
	: "g2", "memory", "cc");

	/* clang-format on */
}


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	/* clang-format off */

	__asm__ volatile(" \
	stbar; \
	stub %%g0, [%0]; \
	rd %%psr, %%g2; \
	and %%g2, "XSTR(PSR_CWP) ", %%g2; \
	ld [%1], %%g3; \
	andn %%g3, "XSTR(PSR_CWP) ", %%g3; \
	or %%g2, %%g3, %%g2; \
	wr %%g2, %%psr; \
	nop; \
	nop; \
	nop; \
	"
	:
	: "r"(&spinlock->lock), "r"(sc)
	: "g2", "g3", "memory");

	/* clang-format on */
}


void _hal_spinlockCreate(spinlock_t *spinlock, const char *name)
{
	spinlock->lock = 0u;
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


void _hal_spinlockInit(void)
{
	spinlock_common.first = NULL;
	_hal_spinlockCreate(&spinlock_common.spinlock, "spinlock_common.spinlock");
}
