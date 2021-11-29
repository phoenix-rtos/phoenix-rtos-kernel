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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../hal/hal.h"


int lib_rand(unsigned int *seedp)
{
	*seedp = (*seedp * 1103515245 + 12345);
	return((unsigned)(*seedp / 2));
}
