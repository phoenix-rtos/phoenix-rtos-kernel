/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "cpu.h"
#include "spinlock.h"
#include "interrupts.h"


struct {
	intr_handler_t wakeuph;
	intr_handler_t timerh;
	spinlock_t lock;
} timer_common;



extern void _end(void);


/* TODO */
void hal_setWakeup(u32 when)
{

}


/* TODO */
time_t hal_getTimer(void)
{
	return 0;
}


/* TODO */
void _timer_init(u32 interval)
{

}
