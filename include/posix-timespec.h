/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility definitions - timespec
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_POSIX_TIMESPEC_H_
#define _PHOENIX_POSIX_TIMESPEC_H_


#include "types.h"


struct timespec {
	time_t tv_sec;
	long tv_nsec;
};


#endif
