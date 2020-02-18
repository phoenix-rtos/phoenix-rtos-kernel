/*
 * Phoenix-RTOS
 *
 * libphoenix
 *
 * time.h
 *
 * Copyright 2020 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_TIME_H_
#define _PHOENIX_TIME_H_

#include "types.h"

#define TIMER_ABSTIME 1

struct timespec {
	time_t tv_sec;
	long tv_nsec;
};

#endif
