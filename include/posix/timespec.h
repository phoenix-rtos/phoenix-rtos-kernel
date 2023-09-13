/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX struct timespec
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_TIMESPEC_H_
#define _PHOENIX_POSIX_TIMESPEC_H_

#include "types.h"


struct timespec {
	time_t tv_sec; /* Seconds */
	long tv_nsec;  /* Nanoseconds */
};


#endif
