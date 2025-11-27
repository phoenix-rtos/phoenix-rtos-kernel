/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2001, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_IA32_H_
#define _PH_HAL_IA32_H_

#include "hal/types.h"
/* io access */


/* parasoft-begin-suppress MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */

static inline u8 hal_inb(u16 addr)
{
	u8 b;

	/* clang-format off */
	__asm__ volatile (
		"inb %1, %0\n\t"
	: "=a" (b)
	: "d" (addr)
	: );
	/* clang-format on */
	return b;
}


static inline void hal_outb(u16 addr, u8 b)
{
	/* clang-format off */
	__asm__ volatile (
		"outb %1, %0"
	:
	: "d" (addr), "a" (b)
	: );
	/* clang-format on */

	return;
}


static inline u32 hal_inl(u16 addr)
{
	u32 l;

	/* clang-format off */
	__asm__ volatile (
		"inl %1, %0\n\t"
	: "=a" (l)
	: "d" (addr)
	: );
	/* clang-format on */

	return l;
}


static inline void hal_outl(u16 addr, u32 l)
{
	/* clang-format off */
	__asm__ volatile (
		"outl %1, %0"
	:
	: "d" (addr), "a" (l)
	: );
	/* clang-format on */

	return;
}


/* memory management */


static inline void hal_cpuSwitchSpace(addr_t cr3)
{
	/* clang-format off */
	__asm__ volatile (
		"movl %0, %%cr3"
	:
	: "r" (cr3)
	: "memory");
	/* clang-format on */

	return;
}

#endif

/* parasoft-end-suppress MISRAC2012-DIR_4_3 */
