/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX struct timeval
 *
 * Copyright 2021 Phoenix Systems
 * Author: Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_TIMEVAL_H_
#define _PHOENIX_POSIX_TIMEVAL_H_

#include "types.h"


struct timeval {
	time_t tv_sec;       /* Seconds */
	suseconds_t tv_usec; /* Microseconds */
};


#endif
