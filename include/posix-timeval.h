/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * POSIX-compatibility definitions - timeval
 *
 * Copyright 2026 Phoenix Systems
 * Author: Ziemowit Leszczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_POSIX_TIMEVAL_H_
#define _PH_POSIX_TIMEVAL_H_


#include "types.h"


struct timeval {
	time_t tv_sec;
	suseconds_t tv_usec;
};


#endif
