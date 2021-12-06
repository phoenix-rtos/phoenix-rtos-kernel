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

#include "../string.h"


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


size_t hal_strlen(const char *s)
{
	size_t k;

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


int hal_strncmp(const char *s1, const char *s2, size_t count)
{
	size_t k;

	for (k = 0; k < count && *s1 && *s2 && (*s1 == *s2); ++k, ++s1, ++s2);

	if (k == count || (!*s1 && !*s2))
		return 0;

	return (*s1 < *s2) ? -k - 1 : k + 1;
}


char *hal_strcpy(char *dest, const char *src)
{
	int i = 0;

	do {
		dest[i] = src[i];
	} while(src[i++] != '\0');

	return dest;
}


char *hal_strncpy(char *dest, const char *src, size_t n)
{
	int i = 0;

	if (n == 0)
		return dest;

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
	unsigned int k, m;
	unsigned long l;

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
