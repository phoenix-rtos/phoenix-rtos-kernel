/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL basic routines
 *
 * Copyright 2017 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Artur Wodejko, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_BASE_H_
#define _HAL_BASE_H_

#include "cpu.h"


static inline void hal_memcpy(void *dst, const void *src, unsigned int l)
{
	__asm__ volatile
	(" \
		mov r1, %2; \
		mov r3, %1; \
		mov r4, %0; \
		orr r2, r3, r4; \
		ands r2, #3; \
		bne 2f; \
	1: \
		cmp r1, #4; \
		ittt hs; \
		ldrhs r2, [r3], #4; \
		strhs r2, [r4], #4; \
		subshs r1, #4; \
		bhs 1b; \
	2: \
		cmp r1, #0; \
		ittt ne; \
		ldrbne r2, [r3], #1; \
		strbne r2, [r4], #1; \
		subsne r1, #1; \
		bne 2b"
	:
	: "r" (dst), "r" (src), "r" (l)
	: "r1", "r2", "r3", "r4", "memory", "cc");
}


static inline int hal_memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	int res = 0;

	__asm__ volatile
	(" \
		mov r1, %1; \
		mov r2, %2; \
		mov r3, %3; \
	1: \
		cmp r3, #0; \
		beq 3f; \
		sub r3, #1; \
		ldrb r4, [r1], #1; \
		ldrb r5, [r2], #1; \
		cmp r4, r5; \
		beq 1b; \
		blo 2f; \
		mov %0, #1; \
		b 3f; \
	2: \
		mov %0, #-1; \
	3: "
	: "+r" (res)
	: "r" (ptr1), "r" (ptr2), "r" (num)
	: "r1", "r2", "r3", "r4", "r5", "cc");

	return res;
}


static inline void hal_memset(void *dst, int v, unsigned int l)
{
	__asm__ volatile
	(" \
		mov r1, %2; \
		mov r3, %1; \
		orr r3, r3, r3, lsl #8; \
		orr r3, r3, r3, lsl #16; \
		mov r4, %0; \
		ands r2, r4, #3; \
		bne 2f; \
	1: \
		cmp r1, #4; \
		itt hs; \
		strhs r3, [r4], #4; \
		subshs r1, #4; \
		bhs 1b; \
	2: \
		cmp r1, #0; \
		itt ne; \
		strbne r3, [r4], #1; \
		subsne r1, #1; \
		bne 2b"
	:
	: "r" (dst), "r" (v & 0xff), "r" (l)
	: "r1", "r2", "r3", "r4", "memory", "cc");
}


static inline unsigned int hal_strlen(const char *s)
{
	unsigned int k = 0;

	__asm__ volatile
	(" \
	1: \
		ldrb r1, [%1, %0]; \
		add %0, #1; \
		cmp r1, #0; \
		bne 1b; \
		sub %0, #1;"
	: "+r" (k)
	: "r" (s)
	: "r1", "cc");

	return k;
}


static inline int hal_strcmp(const char *s1, const char *s2)
{
	int res = 0;

	__asm__ volatile
	(" \
		mov r1, %1; \
		mov r2, %2; \
	1: \
		ldrb r3, [r1], #1; \
		ldrb r4, [r2], #1; \
		cmp r3, #0; \
		beq 2f; \
		cmp r3, r4; \
		beq 1b; \
		blo 3f; \
		mov %0, #1; \
		b 4f; \
	2: \
		cmp r4, #0; \
		beq 4f; \
	3: \
		mov %0, #-1; \
	4: "
	: "+r" (res)
	: "r" (s1), "r" (s2)
	: "r1", "r2", "r3", "r4", "cc");

	return res;
}


static inline int hal_strncmp(const char *s1, const char *s2, unsigned int count)
{
	int res = 0;

	__asm__ volatile
	(" \
		mov r1, %1; \
		mov r2, %2; \
		mov r5, %3; \
	1: \
		cmp r5, #0; \
		beq 4f; \
		sub r5, #1; \
		ldrb r3, [r1], #1; \
		ldrb r4, [r2], #1; \
		cmp r3, #0; \
		beq 2f; \
		cmp r3, r4; \
		beq 1b; \
		blo 3f; \
		mov %0, #1; \
		b 4f; \
	2: \
		cmp r4, #0; \
		beq 4f; \
	3: \
		mov %0, #-1; \
	4: "
	: "+r" (res)
	: "r" (s1), "r" (s2), "r" (count)
	: "r1", "r2", "r3", "r4", "r5", "cc");

	return res;
}


static inline char *hal_strcpy(char *dest, const char *src)
{
	__asm__ volatile
	(" \
		mov r2, %0; \
		mov r3, %1; \
		ldrb r1, [r3], #1; \
	1: \
		strb r1, [r2], #1; \
		cmp r1, #0; \
		itt ne; \
		ldrbne r1, [r3], #1; \
		bne 1b"
	:
	: "r" (dest), "r" (src)
	: "r1", "r2", "r3", "memory", "cc");

	return dest;
}


static inline char *hal_strncpy(char *dest, const char *src, size_t n)
{
	__asm__ volatile
	(" \
		mov r2, %2; \
		mov r3, %0; \
		mov r4, %1; \
		ldrb r1, [r4], #1; \
	1: \
		cmp r2, #0; \
		beq 2f; \
		sub r2, #1; \
		strb r1, [r3], #1; \
		cmp r1, #0; \
		itt ne; \
		ldrbne r1, [r4], #1; \
		bne 1b; \
	2:"
	:
	: "r" (dest), "r" (src), "r" (n)
	: "r1", "r2", "r3", "r4", "memory", "cc");

	return dest;
}


#endif
