/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Binary search
 *
 * Copyright 2012 Phoenix Systems
 * Author: Paweł Kołodziej
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 *
 */

#include "../hal/hal.h"
#include "bsearch.h"


void *lib_bsearch(void *key, void *base, size_t nmemb, size_t size, int (*compar)(void *, void *))
{
	int l = 0, r = nmemb - 1, m;
	int cmp;

	if (nmemb == 0)
		return NULL;

	while (l <= r) {
		m = (l + r) / 2;

		cmp = compar(key, base + m * size);

		if (cmp == 0)
			return base + m * size;

		if (cmp > 0)
			l = m + 1;
		else
			r = m - 1;
	}
	return NULL;
}
