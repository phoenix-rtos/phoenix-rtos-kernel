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

#ifndef _HAL_SPINLOCK_H_
#define _HAL_SPINLOCK_H_

#ifndef __ASSEMBLY__

#include "cpu.h"


typedef struct _spinlock_t {
	const char *name;
	struct _spinlock_t *next;
	struct _spinlock_t *prev;

	u8 lock;
	u8 cflags;
} __attribute__((packed)) spinlock_t;


static inline void hal_spinlockSet(spinlock_t *spinlock)
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
		cmp r1,	#0; \
		bne 1b; \
		dmb"
	:
	: "r" (&spinlock->cflags), "r" (&spinlock->lock)
	: "r1", "r2", "memory", "cc");
}


static inline void hal_spinlockClear(spinlock_t *spinlock)
{
	__asm__ volatile (" \
		ldrexb r1, [%0]; \
		add r1, r1, #1;	\
		dmb; \
		strb r1, [%0];  \
		ldrb r1, [%1]; \
		msr cpsr_c, r1;"
	:
	: "r" (&spinlock->lock), "r" (&spinlock->cflags)
	: "r1", "r2", "memory");
}


extern void hal_spinlockCreate(spinlock_t *spinlock, const char *name);


extern void hal_spinlockDestroy(spinlock_t *spinlock);


extern void _hal_spinlockInit(void);

#endif

#endif
