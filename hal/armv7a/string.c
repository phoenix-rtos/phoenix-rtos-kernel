/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL basic routines
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../string.h"


void *hal_memcpy(void *dst, const void *src, unsigned int l)
{
	__asm__ volatile
	(" \
		orr r3, %0, %1; \
		lsls r3, r3, #30; \
		bne 2f; \
	1: \
		cmp %2, #4; \
		ittt hs; \
		ldrhs r3, [%1], #4; \
		strhs r3, [%0], #4; \
		subshs %2, #4; \
		bhs 1b; \
	2: \
		cmp %2, #0; \
		ittt ne; \
		ldrbne r3, [%1], #1; \
		strbne r3, [%0], #1; \
		subsne %2, #1; \
		bne 2b"
	: "+r" (dst), "+r" (src), "+r" (l)
	:
	: "r3", "memory", "cc");

	return dst;
}


int hal_memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	int res = 0;

	__asm__ volatile
	(" \
	1: \
		cmp %3, #0; \
		beq 3f; \
		sub %3, #1; \
		ldrb r3, [%1], #1; \
		ldrb r4, [%2], #1; \
		cmp r3, r4; \
		beq 1b; \
		blo 2f; \
		mov %0, #1; \
		b 3f; \
	2: \
		mov %0, #-1; \
	3: "
	: "+r" (res), "+r" (ptr1), "+r" (ptr2), "+r" (num)
	:
	: "r3", "r4", "memory", "cc");

	return res;
}


void *hal_memset(void *dst, int v, unsigned int l)
{
	unsigned int v1 = v & 0xff;
	unsigned int tmp;

	tmp = (v1 << 8) | v1;
	tmp |= (tmp << 16);

	__asm__ volatile
	(" \
		lsls r3, %0, #30; \
		bne 2f; \
	1: \
		cmp %2, #4; \
		itt hs; \
		strhs %1, [%0], #4; \
		subshs %2, #4; \
		bhs 1b; \
	2: \
		cmp %2, #0; \
		itt ne; \
		strbne %1, [%0], #1; \
		subsne %2, #1; \
		bne 2b"
	: "+r"(dst), "+r" (tmp), "+r" (l)
	:
	: "r3", "memory", "cc");

	return dst;
}


unsigned int hal_strlen(const char *s)
{
	unsigned int k = 0;

	__asm__ volatile
	(" \
	1: \
		ldrb r1, [%1, %0]; \
		cbz r1, 2f; \
		add %0, #1; \
		b 1b; \
	2:"
	: "+r" (k), "+r" (s)
	:
	: "r1", "memory", "cc");

	return k;
}


int hal_strcmp(const char *s1, const char *s2)
{
	int res = 0;

	__asm__ volatile
	(" \
	1: \
		ldrb r2, [%1], #1; \
		ldrb r3, [%2], #1; \
		cbz r2, 2f; \
		cmp r2, r3; \
		beq 1b; \
		blo 3f; \
		mov %0, #1; \
		b 4f; \
	2: \
		cmp r3, #0; \
		beq 4f; \
	3: \
		mov %0, #-1; \
	4: "
	: "+r" (res), "+r" (s1), "+r" (s2)
	:
	: "r2", "r3", "memory", "cc");

	return res;
}


int hal_strncmp(const char *s1, const char *s2, unsigned int count)
{
	int res = 0;

	__asm__ volatile
	(" \
	1: \
		cmp %3, #0; \
		beq 4f; \
		sub %3, #1; \
		ldrb r3, [%1], #1; \
		ldrb r4, [%2], #1; \
		cbz r3, 2f; \
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
	: "+r" (res), "+r" (s1), "+r" (s2), "+r" (count)
	:
	: "r3", "r4", "memory", "cc");

	return res;
}


char *hal_strcpy(char *dest, const char *src)
{
	char *p = dest;

	__asm__ volatile
	(" \
	1: \
		ldrb r3, [%1], #1; \
		strb r3, [%0], #1; \
		cmp r3, #0; \
		bne 1b"
	: "+r" (p), "+r" (src)
	:
	: "r3", "memory", "cc");

	return dest;
}


char *hal_strncpy(char *dest, const char *src, size_t n)
{
	char *p = dest;

	__asm__ volatile
	(" \
		cmp %2, #0; \
		beq 2f; \
	1: \
		ldrb r3, [%1], #1; \
		strb r3, [%0], #1; \
		cbz r3, 2f; \
		subs %2, #1; \
		bne 1b; \
	2:"
	: "+r" (p), "+r" (src), "+r" (n)
	:
	: "r3", "memory", "cc");

	return dest;
}


int hal_i2s(char *prefix, char *s, unsigned int i, unsigned char b, char zero)
{
	static const char digits[] = "0123456789abcdef";
	char c;
	unsigned int l, k, m;

	m = hal_strlen(prefix);
	hal_memcpy(s, prefix, m);

	for (k = m, l = (unsigned int)-1; l; i /= b, l /= b) {
		if (!zero && !i)
			break;
		s[k++] = digits[i % b];
	}

	l = k--;

	while (k > m) {
		c = s[m];
		s[m++] = s[k];
		s[k--] = c;
	}

	return l;
}
