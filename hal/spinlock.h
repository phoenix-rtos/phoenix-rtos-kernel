/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Spinlock
 *
 * Copyright 2014, 2017 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_SPINLOCK_H_
#define _PH_HAL_SPINLOCK_H_

#include <arch/spinlock.h>


void hal_spinlockSet(spinlock_t *spinlock, spinlock_ctx_t *sc);


void hal_spinlockClear(spinlock_t *spinlock, spinlock_ctx_t *sc);


void hal_spinlockCreate(spinlock_t *spinlock, const char *name);


void hal_spinlockDestroy(spinlock_t *spinlock);


void _hal_spinlockInit(void);


#endif
