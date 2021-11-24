/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2018 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_TIMER_H_
#define _HAL_TIMER_H_

#ifndef __ASSEMBLY__

#include <arch/types.h>


extern time_t hal_getTimer(void);


extern void hal_setWakeup(u32 when);


extern void _timer_init(u32 interval);


#endif

#endif
