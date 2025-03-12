/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL basic routines
 *
 * Copyright 2017, 2018, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/string.h"


int hal_memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	int res = 0;

	/* clang-format off */
	__asm__ volatile (
	"1:\n\t"
		"cmp %3, #0\n\t"
		"beq 3f\n\t"
		"sub %3, #1\n\t"
		"ldrb r3, [%1], #1\n\t"
		"ldrb r4, [%2], #1\n\t"
		"cmp r3, r4\n\t"
		"beq 1b\n\t"
		"blo 2f\n\t"
		"mov %0, #1\n\t"
		"b 3f\n\t"
	"2:\n\t"
		"mov %0, #-1\n\t"
	"3: "
	: "+r" (res), "+r" (ptr1), "+r" (ptr2), "+r" (num)
	:
	: "r3", "r4", "memory", "cc");
	/* clang-format on */

	return res;
}


unsigned int hal_strlen(const char *s)
{
	unsigned int k = 0;

	/* clang-format off */
	__asm__ volatile (
	"1:\n\t"
		"ldrb r1, [%1, %0]\n\t"
		"cbz r1, 2f\n\t"
		"add %0, #1\n\t"
		"b 1b\n\t"
	"2:"
	: "+r" (k), "+r" (s)
	:
	: "r1", "memory", "cc");
	/* clang-format on */

	return k;
}


int hal_strcmp(const char *s1, const char *s2)
{
	int res = 0;

	/* clang-format off */
	__asm__ volatile (
	"1:\n\t"
		"ldrb r2, [%1], #1\n\t"
		"ldrb r3, [%2], #1\n\t"
		"cbz r2, 2f\n\t"
		"cmp r2, r3\n\t"
		"beq 1b\n\t"
		"blo 3f\n\t"
		"mov %0, #1\n\t"
		"b 4f\n\t"
	"2:\n\t"
		"cmp r3, #0\n\t"
		"beq 4f\n\t"
	"3:\n\t"
		"mov %0, #-1\n\t"
	"4: "
	: "+r" (res), "+r" (s1), "+r" (s2)
	:
	: "r2", "r3", "memory", "cc");
	/* clang-format on */

	return res;
}


int hal_strncmp(const char *s1, const char *s2, unsigned int count)
{
	int res = 0;

	/* clang-format off */
	__asm__ volatile (
	"1:\n\t"
		"cmp %3, #0\n\t"
		"beq 4f\n\t"
		"sub %3, #1\n\t"
		"ldrb r3, [%1], #1\n\t"
		"ldrb r4, [%2], #1\n\t"
		"cbz r3, 2f\n\t"
		"cmp r3, r4\n\t"
		"beq 1b\n\t"
		"blo 3f\n\t"
		"mov %0, #1\n\t"
		"b 4f\n\t"
	"2:\n\t"
		"cmp r4, #0\n\t"
		"beq 4f\n\t"
	"3:\n\t"
		"mov %0, #-1\n\t"
	"4: "
	: "+r" (res), "+r" (s1), "+r" (s2), "+r" (count)
	:
	: "r3", "r4", "memory", "cc");
	/* clang-format on */

	return res;
}


char *hal_strcpy(char *dest, const char *src)
{
	char *p = dest;

	/* clang-format off */
	__asm__ volatile (
	"1:\n\t"
		"ldrb r3, [%1], #1\n\t"
		"strb r3, [%0], #1\n\t"
		"cmp r3, #0\n\t"
		"bne 1b"
	: "+r" (p), "+r" (src)
	:
	: "r3", "memory", "cc");
	/* clang-format on */

	return dest;
}


char *hal_strncpy(char *dest, const char *src, size_t n)
{
	char *p = dest;

	/* clang-format off */
	__asm__ volatile (
		"cmp %2, #0\n\t"
		"beq 2f\n\t"
	"1:\n\t"
		"ldrb r3, [%1], #1\n\t"
		"strb r3, [%0], #1\n\t"
		"cbz r3, 2f\n\t"
		"subs %2, #1\n\t"
		"bne 1b\n\t"
	"2:"
	: "+r" (p), "+r" (src), "+r" (n)
	:
	: "r3", "memory", "cc");
	/* clang-format on */

	return dest;
}


unsigned long hal_i2s(const char *prefix, char *s, unsigned long i, unsigned char b, char zero)
{
	static const char digits[] = "0123456789abcdef";
	char c;
	unsigned long l, k, m;

	m = hal_strlen(prefix);
	hal_memcpy(s, prefix, m);

	for (k = m, l = (unsigned long)-1; l; i /= b, l /= b) {
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
