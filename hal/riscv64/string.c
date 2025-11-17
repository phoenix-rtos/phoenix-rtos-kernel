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

#include "hal/string.h"


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


size_t hal_strlen(const char *s)
{
	size_t k = 0;

	while (*s != '\0') {
		k++;
		s++;
	}

	return k;
}


int hal_strcmp(const char *s1, const char *s2)
{
	const u8 *us1 = (const u8 *)s1;
	const u8 *us2 = (const u8 *)s2;
	const u8 *p;
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
	const u8 *us1 = (const u8 *)s1;
	const u8 *us2 = (const u8 *)s2;
	size_t k = 0;

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

	/* parasoft-begin-suppress MISRAC2012-RULE_18_1 "src is assumed to end with a null-byte" */
	do {
		dest[i] = src[i];
	} while (src[i++] != '\0');
	/* parasoft-end-suppress MISRAC2012-RULE_18_1 */

	return dest;
}


char *hal_strncpy(char *dest, const char *src, size_t n)
{
	size_t i = 0;

	if (n == 0U) {
		return dest;
	}

	/* parasoft-begin-suppress MISRAC2012-RULE_18_1 "src is assumed to end with a null-byte" */
	do {
		dest[i] = src[i];
		i++;
	} while (i < n && src[i - 1U] != '\0');
	/* parasoft-end-suppress MISRAC2012-RULE_18_1 */

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
