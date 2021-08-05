/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_TIMEDEV_H_
#define _HAL_TIMEDEV_H_

#ifndef __ASSEMBLY__

#include "cpu.h"

#define TIMER_US2CYC(x) (x)
#define TIMER_CYC2US(x) (x)


extern time_t hal_getTimer(void);


extern int timer_reschedule(unsigned int n, cpu_context_t *ctx, void *arg);


extern void _timer_init(u32 interval);


#endif

#endif
