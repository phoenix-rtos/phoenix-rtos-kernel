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

#include "hal/hal.h"
#include "bsearch.h"


void *lib_bsearch(void *key, void *base, size_t nmemb, size_t size, int (*compar)(void *n1, void *n2))
{
	unsigned int l = 0, r = nmemb - 1U, m;
	int cmp;

	if (nmemb == 0U) {
		return NULL;
	}

	while (l <= r) {
		m = (l + r) / 2U;

		cmp = compar(key, base + m * size);

		if (cmp == 0) {
			return base + m * size;
		}

		if (cmp > 0) {
			l = m + 1U;
		}
		else {
			r = m - 1U;
		}
	}
	return NULL;
}
