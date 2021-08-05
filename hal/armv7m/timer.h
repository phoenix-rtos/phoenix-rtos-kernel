/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Author: Jakub Sejdak
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


extern void timer_jiffiesAdd(time_t t);


extern time_t hal_getTimer(void);


extern void _timer_init(u32 interval);


#endif

#endif
