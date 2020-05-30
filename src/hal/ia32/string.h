/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL basic routines
 *
 * Copyright 2012 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_BASE_H_
#define _HAL_BASE_H_

#ifndef __ASSEMBLY__

#include "cpu.h"


static inline void hal_memcpy(void *to, const void *from, unsigned int n)
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
	: "g" (n), "g" (to), "g" (from)
	: "ecx", "edx", "esi", "edi", "cc", "memory");
}


static inline void hal_memset(void *where, u8 v, unsigned int n)
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
	: "+d" (n)
	: "m" (v), "m" (where)
	: "eax", "ebx", "cc", "ecx", "edi" ,"memory");
}


static inline void hal_memsetw(void *where, u16 v, unsigned int n)
{
	__asm__ volatile
	(" \
		cld; \
		xorl %%eax, %%eax; \
		movw %1, %%ax; \
		movl %2, %%edi; \
		movl %0, %%ecx; \
		rep; stosw"
	: "+d" (n)
	: "g" (v), "m" (where)
	: "eax", "ecx", "edi", "cc", "memory");
}


#endif


static inline unsigned int hal_strlen(const char *s)
{
	unsigned int k;

	for (k = 0; *s; s++, k++);
	return k;
}


static inline int hal_strcmp(const char *s1, const char *s2)
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


static inline int hal_strncmp(const char *s1, const char *s2, unsigned int count)
{
	unsigned int k;

	for (k = 0; k < count && *s1 && *s2 && (*s1 == *s2); ++k, ++s1, ++s2);

	if (k == count || (!*s1 && !*s2))
		return 0;

	return (*s1 < *s2) ? -k - 1 : k + 1;
}


static inline char *hal_strcpy(char *dest, const char *src)
{
	int i = 0;

	do {
		dest[i] = src[i];
	} while(src[i++] != '\0');

	return dest;
}


static inline char *hal_strncpy(char *dest, const char *src, size_t n)
{
	int i = 0;

	do {
		dest[i] = src[i];
		i++;
	} while (i < n && src[i - 1] != '\0');

	return dest;
}


static inline unsigned int hal_i2s(char *prefix, char *s, unsigned int i, unsigned char b, char zero)
{
	char digits[] = "0123456789abcdef";
	char c;
	unsigned int l, k, m;

	m = 0;
	while (*prefix) {
		s[m] = *prefix++;
		m++;
	}

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


#endif
