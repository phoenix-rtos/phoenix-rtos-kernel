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

#ifndef _HAL_TIMER_H_
#define _HAL_TIMER_H_


#include "cpu.h"
#include "interrupts.h"

extern time_t hal_timerGetUs(void);


extern void hal_timerSetWakeup(u32 when);


extern int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h);


extern void _hal_timerInit(u32 interval);

#endif
