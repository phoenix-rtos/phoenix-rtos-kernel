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

#ifndef _HAL_IA32_H_
#define _HAL_IA32_H_

#include <arch/types.h>

/* io access */


static inline u8 hal_inb(void *addr)
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


static inline void hal_outb(void *addr, u8 b)
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


static inline u16 hal_inw(void *addr)
{
	u16 w;

	/* clang-format off */
	__asm__ volatile (
		"inw %1, %0\n\t"
	: "=a" (w)
	: "d" (addr)
	: );
	/* clang-format on */

	return w;
}


static inline void hal_outw(void *addr, u16 w)
{
	/* clang-format off */
	__asm__ volatile (
		"outw %1, %0"
	:
	: "d" (addr), "a" (w)
	: );
	/* clang-format on */

	return;
}


static inline u32 hal_inl(void *addr)
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


static inline void hal_outl(void *addr, u32 l)
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


static inline void hal_wrmsr(u32 id, u64 v)
{
	__asm__ volatile ("wrmsr":: "c" (id), "A" (v));
}


static inline u64 hal_rdmsr(u32 id)
{
	u64 v;

	__asm__ volatile ("rdmsr" : "=A" (v) : "c" (id));
	return v;
}


/* memory management */


static inline void hal_cpuFlushTLB(void *vaddr)
{
	unsigned long tmpreg;

	do {

		/* clang-format off */
		__asm__ volatile (
			"movl %%cr3, %0\n\t"
			"movl %0, %%cr3"
		: "=r" (tmpreg)
		:
		: "memory");
		/* clang-format on */

	} while (0);

	return;
}


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
