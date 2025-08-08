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


static int strtoul_isalnum(u8 c)
{
	if ((c >= '0') && (c <= '9')) {
		return 1;
	}

	/* test letter */
	c &= ~0x20U;
	if ((c >= 'A') && (c <= 'Z')) {
		return 1;
	}

	return 0;
}


unsigned long lib_strtoul(u8 *nptr, u8 **endptr, int base)
{
	unsigned long t, v = 0;

	if ((base == 16) && (nptr[0] == '0') && (nptr[1] == 'x')) {
		nptr += 2;
	}

	while (strtoul_isalnum(*nptr) != 0) {
		t = *nptr - '0';
		if (t > 9) {
			t = (*nptr | 0x20U) - 'a' + 10;
		}

		if (t >= base) {
			break;
		}

		v = (v * base) + t;

		++nptr;
	}

	if (endptr != NULL) {
		*endptr = nptr;
	}

	return v;
}


long lib_strtol(u8 *nptr, u8 **endptr, int base)
{
	int sign = 1;

	if (*nptr == '-') {
		sign = -1;
		++nptr;
	}

	return sign * (long)lib_strtoul(nptr, endptr, base);
}
