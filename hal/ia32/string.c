/*
 * Phoenix-RTOS
 *
 * Operating system loader
 *
 * HAL basic routines
 *
 * Copyright 2012, 2021 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/string.h"


void hal_memcpy(void *dst, const void *src, size_t l)
{
	/* clang-format off */
	__asm__ volatile (
		"cld\n\t"
		"movl %0, %%ecx\n\t"
		"movl %%ecx, %%edx\n\t"
		"andl $3, %%edx\n\t"
		"shrl $2, %%ecx\n\t"
		"movl %1, %%edi\n\t"
		"movl %2, %%esi\n\t"
		"rep movsl\n\t"
		"movl %%edx, %%ecx\n\t"
		"rep movsb"
	:
	: "g" (l), "g" (dst), "g" (src)
	: "ecx", "edx", "esi", "edi", "cc", "memory");
	/* clang-format on */
}


int hal_memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	unsigned int i;

	for (i = 0; i < num; ++i) {
		if (((const u8 *)ptr1)[i] < ((const u8 *)ptr2)[i]) {
			return -1;
		}
		else if (((const u8 *)ptr1)[i] > ((const u8 *)ptr2)[i]) {
			return 1;
		}
		else {
			/* No action required */
		}
	}

	return 0;
}


void hal_memset(void *dst, int v, size_t l)
{
	/* clang-format off */
	__asm__ volatile (
		"cld\n\t"
		"movl %0, %%ecx\n\t"
		"movl %%ecx, %%edx\n\t"
		"andl $3, %%edx\n\t"
		"shrl $2, %%ecx\n\t"
		"\n"
		"xorl %%eax, %%eax\n\t"
		"movb %1, %%al\n\t"
		"movl %%eax, %%ebx\n\t"
		"shll $8, %%ebx\n\t"
		"orl %%ebx, %%eax\n\t"
		"movl %%eax, %%ebx\n\t"
		"shll $16, %%ebx\n\t"
		"orl %%ebx, %%eax\n\t"
		"\n"
		"movl %2, %%edi\n\t"
		"rep stosl\n\t"
		"movl %%edx, %%ecx\n\t"
		"rep stosb"
	: "+d" (l)
	: "m" (v), "m" (dst)
	: "eax", "ebx", "cc", "ecx", "edi" ,"memory");
	/* clang-format on */
}


size_t hal_strlen(const char *s)
{
	unsigned int k;

	for (k = 0; *s != '\0'; s++) {
		k++;
	}

	return k;
}


int hal_strcmp(const char *s1, const char *s2)
{
	const unsigned char *us1 = (const unsigned char *)s1;
	const unsigned char *us2 = (const unsigned char *)s2;
	const unsigned char *p;
	unsigned int k = 0;

	for (p = us1; *p != 0U; p++) {

		if (*p < *(us2 + k)) {
			return -1;
		}
		else if (*p > *(us2 + k)) {
			return 1;
		}
		else {
			/* No action required */
		}

		k++;
	}

	if (*p != *(us2 + k)) {
		return -1;
	}

	return 0;
}


int hal_strncmp(const char *s1, const char *s2, size_t n)
{
	const unsigned char *us1 = (const unsigned char *)s1;
	const unsigned char *us2 = (const unsigned char *)s2;
	unsigned int k = 0;

	while (k < n && (*us1 != 0U) && (*us2 != 0U) && (*us1 == *us2)) {
		++k;
		++us1;
		++us2;
	}

	if (k == n || ((*us1 == 0U) && (*us2 == 0U))) {
		return 0;
	}

	return (*us1 < *us2) ? -((int)k) - 1 : (int)k + 1;
}


char *hal_strcpy(char *dest, const char *src)
{
	int i = 0;

	do {
		dest[i] = src[i];
	} while (src[i++] != '\0');

	return dest;
}


char *hal_strncpy(char *dest, const char *src, size_t n)
{
	unsigned int i = 0;

	if (n == 0U) {
		return dest;
	}

	do {
		dest[i] = src[i];
		i++;
	} while (i < n && src[i - 1U] != '\0');

	return dest;
}


unsigned long hal_i2s(const char *prefix, char *s, unsigned long i, u8 b, u8 zero)
{
	static const char digits[] = "0123456789abcdef";
	char c;
	unsigned long l, k, m;

	m = hal_strlen(prefix);
	hal_memcpy(s, prefix, m);
	k = m;

	for (l = (unsigned long)-1; l != 0U; l /= b) {
		if ((zero == 0U) && (i == 0U)) {
			break;
		}
		s[k++] = digits[i % b];
		i /= b;
	}

	if (k != 0U) {
		l = k--;
	}

	while (k > m) {
		c = s[m];
		s[m++] = s[k];
		s[k--] = c;
	}

	return l;
}
