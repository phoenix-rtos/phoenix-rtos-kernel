/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2018, 2021 Phoenix Systems
 * Author: Aleksander Kaminski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_TIMER_H_
#define _PH_HAL_TIMER_H_


#include "cpu.h"
#include "interrupts.h"

time_t hal_timerGetUs(void);


void hal_timerSetWakeup(u32 waitUs);


int hal_timerRegister(intrFn_t f, void *data, intr_handler_t *h);


void _hal_timerInit(u32 interval);


char *hal_timerFeatures(char *features, unsigned int len);

#endif
