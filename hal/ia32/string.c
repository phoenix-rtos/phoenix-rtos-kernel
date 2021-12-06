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

#include "../string.h"


void hal_memcpy(void *dst, const void *src, size_t l)
{
	__asm__ volatile
	(" \
		cld; \
		movl %0, %%ecx; \
		movl %%ecx, %%edx; \
		andl $3, %%edx; \
		shrl $2, %%ecx; \
		movl %1, %%edi; \
		movl %2, %%esi; \
		rep; movsl; \
		movl %%edx, %%ecx; \
		rep; movsb"
	:
	: "g" (l), "g" (dst), "g" (src)
	: "ecx", "edx", "esi", "edi", "cc", "memory");
}


int hal_memcmp(const void *ptr1, const void *ptr2, size_t num)
{
	int i;

	for (i = 0; i < num; ++i) {
		if (((const u8 *)ptr1)[i] < ((const u8 *)ptr2)[i])
			return -1;
		else if (((const u8 *)ptr1)[i] > ((const u8 *)ptr2)[i])
			return 1;
	}

	return 0;
}


void hal_memset(void *dst, int v, size_t l)
{
	__asm__ volatile
	(" \
		cld; \
		movl %0, %%ecx; \
		movl %%ecx, %%edx; \
		andl $3, %%edx; \
		shrl $2, %%ecx; \
		\
		xorl %%eax, %%eax; \
		movb %1, %%al; \
		movl %%eax, %%ebx; \
		shll $8, %%ebx; \
		orl %%ebx, %%eax; \
		movl %%eax, %%ebx; \
		shll $16, %%ebx; \
		orl %%ebx, %%eax; \
		\
		movl %2, %%edi; \
		rep; stosl; \
		movl %%edx, %%ecx; \
		rep; stosb"
	: "+d" (l)
	: "m" (v), "m" (dst)
	: "eax", "ebx", "cc", "ecx", "edi" ,"memory");
}


size_t hal_strlen(const char *s)
{
	unsigned int k;

	for (k = 0; *s; s++, k++)
		;

	return k;
}


int hal_strcmp(const char *s1, const char *s2)
{
	const char *p;
	unsigned int k;

	for (p = s1, k = 0; *p; p++, k++) {

		if (*p < *(s2 + k))
			return -1;
		else if (*p > *(s2 + k))
			return 1;
	}

	if (*p != *(s2 + k))
		return -1;

	return 0;
}


int hal_strncmp(const char *s1, const char *s2, unsigned int count)
{
	unsigned int k;

	for (k = 0; k < count && *s1 && *s2 && (*s1 == *s2); ++k, ++s1, ++s2)
		;

	if (k == count || (!*s1 && !*s2))
		return 0;

	return (*s1 < *s2) ? -k - 1 : k + 1;
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
	int i = 0;

	do {
		dest[i] = src[i];
		i++;
	} while (i < n && src[i - 1] != '\0');

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
