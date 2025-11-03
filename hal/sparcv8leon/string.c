/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL basic routines
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/string.h"


void *memcpy(void *dst, const void *src, unsigned int l)
{
	hal_memcpy(dst, src, l);
	return dst;
}


void hal_memcpy(void *dst, const void *src, size_t l)
{
	/* clang-format off */

	__asm__ volatile(" \
		cmp %2, %%g0; \
		bz 3f; \
		nop; \
		or %1, %2, %%g1; \
		or %0, %%g1, %%g1; \
		btst 3, %%g1; \
		be 2f; \
		nop; \
	1: \
		ldub [%1], %%g1; \
		stb %%g1, [%0]; \
		inc %1; \
		inc %0; \
		deccc %2; \
		bnz 1b; \
		nop; \
		ba 3f; \
		nop; \
	2: \
		ld [%1], %%g1; \
		st %%g1, [%0]; \
		add %1, 4, %1; \
		add %0, 4, %0; \
		subcc %2, 4, %2; \
		bnz 2b; \
		nop; \
	3: "
	 : "+r"(dst), "+r"(src), "+r"(l)
	 :
	 : "g1", "memory", "cc");

	/* clang-format on */
}


int hal_memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	int res = 0;

	/* clang-format off */

	__asm__ volatile(" \
	1: \
		cmp %3, %%g0; \
		be 3f; \
		dec %3; \
		ldub [%1], %%g1; \
		ldub [%2], %%g2; \
		inc %1; \
		cmp %%g1, %%g2; \
		be 1b; \
		inc %2; \
		bl 2f; \
		nop; \
		ba 3f; \
		inc %0; \
	2: \
		dec %0; \
	3: "
	 : "+r"(res), "+r"(ptr1), "+r"(ptr2), "+r"(num)
	 :
	 : "g1", "g2", "memory", "cc");

	/* clang-format on */

	return res;
}


void hal_memset(void *dst, int v, size_t l)
{
	/* clang-format off */

	__asm__ volatile(" \
	1: \
		cmp %2, %%g0; \
		be 2f; \
		dec %2; \
		stb %1, [%0]; \
		inc %0; \
		ba 1b; \
		nop; \
	2: "
	 : "+r"(dst), "+r"(v), "+r"(l)
	 :
	 : "memory", "cc");

	/* clang-format on */
}


size_t hal_strlen(const char *s)
{
	size_t l = 0;

	/* clang-format off */

	__asm__ volatile(" \
	1: \
		ldub [%1 + %0], %%g1; \
		cmp %%g1, %%g0; \
		bne,a 1b; \
		inc %0; \
	"
	 : "+r"(l), "+r"(s)
	 :
	 : "g1", "memory", "cc");

	/* clang-format on */

	return l;
}


int hal_strcmp(const char *s1, const char *s2)
{
	int res = 0;

	/* clang-format off */

	__asm__ volatile(" \
	1: \
		ldub [%1], %%g1; \
		ldub [%2], %%g2; \
		cmp %%g1, %%g0; \
		be 2f; \
		inc %1; \
		cmp %%g1, %%g2; \
		be 1b; \
		inc %2; \
		bl 3f; \
		mov 1, %0; \
		ba 4f; \
	2: \
		cmp %%g2, %%g0; \
		be 4f; \
		nop; \
	3: \
		mov -1, %0; \
	4: "
	 : "+r"(res), "+r"(s1), "+r"(s2)
	 :
	 : "g1", "g2", "memory", "cc");

	/* clang-format on */

	return res;
}


int hal_strncmp(const char *s1, const char *s2, size_t n)
{
	int res = 0;

	/* clang-format off */

	__asm__ volatile(" \
	1: \
		cmp %3, %%g0; \
		be 4f; \
		dec %3; \
		ldub [%1], %%g1; \
		ldub [%2], %%g2; \
		cmp %%g1, %%g0; \
		be 2f; \
		inc %1; \
		cmp %%g1, %%g2; \
		be 1b; \
		inc %2; \
		bl 3f; \
		mov 1, %0; \
		ba 4f; \
	2: \
		cmp %%g2, %%g0; \
		be 4f; \
		nop; \
	3: \
		mov -1, %0; \
	4: "
	 : "+r"(res), "+r"(s1), "+r"(s2), "+r"(n)
	 :
	 : "g1", "g2", "memory", "cc");

	/* clang-format on */

	return res;
}


char *hal_strcpy(char *dest, const char *src)
{
	char *p = dest;

	/* clang-format off */

	__asm__ volatile(" \
	1: \
		ldub [%1], %%g1; \
		stb %%g1, [%0]; \
		inc %1; \
		cmp %%g1, %%g0; \
		bne 1b; \
		inc %0; \
	"
	 : "+r"(p), "+r"(src)
	 :
	 : "g1", "memory", "cc");

	/* clang-format on */

	return dest;
}


char *hal_strncpy(char *dest, const char *src, size_t n)
{
	char *p = dest;

	/* clang-format off */

	__asm__ volatile
	(" \
		cmp %2, %%g0; \
		be 3f; \
	1: \
		ldub [%1], %%g1; \
		inc %1; \
		stb %%g1, [%0]; \
		deccc %2; \
		bz 3f; \
		inc %0; \
		cmp %%g1, %%g0; \
		bne 1b; \
		nop; \
	2: \
		stb %%g0, [%0]; \
		deccc %2; \
		bnz 2b; \
		inc %0; \
	3: "
	 : "+r"(p), "+r"(src), "+r"(n)
	 :
	 : "%g1", "memory", "cc");

	/* clang-format on */

	return dest;
}


unsigned long hal_i2s(const char *prefix, char *s, unsigned long i, u8 b, u8 zero)
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
