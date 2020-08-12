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

#include "string.h"


unsigned int hal_i2s(char *prefix, char *s, unsigned int i, unsigned char b, char zero)
{
	static const char digits[] = "0123456789abcdef";
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
