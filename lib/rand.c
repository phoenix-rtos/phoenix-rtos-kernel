/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Standard library - random number generator
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Author: Pawel Kolodziej
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "rand.h"


int lib_rand(unsigned int *seedp)
{
	*seedp = (*seedp * 1103515245U + 12345U);
	return (int)(unsigned int)(*seedp / 2U);
}
