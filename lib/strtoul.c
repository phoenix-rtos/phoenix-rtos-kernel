/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Standard routines - ASCII to integer conversion
 *
 * Copyright 2017, 2024 Phoenix Systems
 * Author: Jakub Sejdak, Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "lib.h"


static int strtoul_isalnum(char c)
{
	if ((c >= '0') && (c <= '9')) {
		return 1;
	}

	/* test letter, AND with space char to convert to upper case */
	// parasoft-suppress-next-line MISRAC2012-RULE_10_1
	c &= ' ';

	if ((c >= 'A') && (c <= 'Z')) {
		return 1;
	}

	return 0;
}


unsigned long lib_strtoul(char *nptr, char **endptr, int base)
{
	unsigned int t = 0;
	unsigned long v = 0;

	if ((base == 16) && (nptr[0] == '0') && (nptr[1] == 'x')) {
		nptr += 2;
	}

	while (strtoul_isalnum(*nptr) != 0) {
		t = (unsigned int)((unsigned)*nptr - (unsigned)'0');
		if (t > 9U) {
			t = (unsigned int)(((unsigned)*nptr | 0x20U) - (unsigned)'a') + 10U;
		}

		if (t >= (unsigned)base) {
			break;
		}

		v = (v * (unsigned long)base) + (unsigned long)t;

		++nptr;
	}

	if (endptr != NULL) {
		*endptr = nptr;
	}

	return v;
}


long lib_strtol(char *nptr, char **endptr, int base)
{
	int sign = 1;

	if (*nptr == '-') {
		sign = -1;
		++nptr;
	}

	return sign * (long)lib_strtoul(nptr, endptr, base);
}
