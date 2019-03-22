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
	unsigned rn = n & 3;
	n /= 4;

	__asm__ volatile
	(" \
		cld; \
		rep; movsl; \
		movl %%edx, %%ecx; \
		rep; movsb"
	: "+c" (n), "+D" (to), "+S" (from)
	: "d" (rn)
	: "cc", "memory");
}


static inline void hal_memset(void *where, u8 v, unsigned int n)
{
	u16 vv = v | (v << 8);
	u32 v4 = vv | (vv << 16);
	unsigned rn = n & 3;
	n /= 4;

	__asm__ volatile
	(" \
		cld; \
		rep; stosl; \
		movl %%edx, %%ecx; \
		rep; stosb"
	: "+c" (n), "+D" (where)
	: "a" (v4), "d" (rn)
	: "cc", "memory");
}


static inline void hal_memsetw(void *where, u16 v, unsigned int n)
{
	u32 vv = v | (v << 16);
	unsigned rn = n & 1;
	n /= 2;

	__asm__ volatile
	(" \
		cld; \
		rep; stosl; \
		movl %%edx, %%ecx; \
		rep; stosw"
	: "+c" (n), "+D" (where)
	: "a" (vv), "d" (rn)
	: "cc", "memory");
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


#endif
