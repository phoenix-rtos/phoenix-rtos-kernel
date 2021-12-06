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

	__asm__ volatile
	(" \
		movl %1, %%edx; \
		inb %%dx, %%al; \
		movb %%al, %0;" \
	:"=b" (b) \
	:"g" (addr) \
	:"edx", "eax");
	return b;
}


static inline void hal_outb(void *addr, u8 b)
{
	__asm__ volatile
	(" \
		movl %0, %%edx; \
		movb %1, %%al; \
		outb %%al, %%dx"
	:
	:"g" (addr), "b" (b)
	:"eax", "edx");

	return;
}


static inline u16 hal_inw(void *addr)
{
	u16 w;

	__asm__ volatile
	(" \
		movl %1, %%edx; \
		inw %%dx, %%ax; \
		movw %%ax, %0;" \
	:"=g" (w) \
	:"g" (addr) \
	:"edx", "eax");

	return w;
}


static inline void hal_outw(void *addr, u16 w)
{
	__asm__ volatile
	(" \
		movl %0, %%edx; \
		movw %1, %%ax; \
		outw %%ax, %%dx"
		:
		:"g" (addr), "g" (w)
		:"eax", "edx");

	return;
}


static inline u32 hal_inl(void *addr)
{
	u32 l;

	__asm__ volatile
	(" \
		movl %1, %%edx; \
		inl %%dx, %%eax; \
		movl %%eax, %0;" \
		:"=g" (l) \
		:"g" (addr) \
		:"eax", "edx", "memory");

	return l;
}


static inline void hal_outl(void *addr, u32 l)
{
	__asm__ volatile
	(" \
		movl %0, %%edx; \
		movl %1, %%eax; \
		outl %%eax, %%dx"
		:
		:"g" (addr), "g" (l)
		:"eax", "edx");

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

		__asm__ volatile
		(" \
			movl %%cr3, %0; \
			movl %0, %%cr3"
			:"=r" (tmpreg)
			:
			:"memory");

	} while (0);

	return;
}


static inline void hal_cpuSwitchSpace(addr_t cr3)
{
	__asm__ volatile
	(" \
		movl %0, %%eax; \
		movl %%eax, %%cr3;"
	:
	:"g" (cr3)
	: "eax", "memory");

	return;
}

#endif
