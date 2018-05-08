/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "cpu.h"
#include "interrupts.h"

#include "../../../include/errno.h"


struct {
	u32 interval;
} timer;


int timer_reschedule(unsigned int n, cpu_context_t *ctx, void *arg)
{
//	timer.scheduler(ctx);	
	return EOK;
}


__attribute__ ((section (".init"))) void _timer_init(u32 interval)
{
	unsigned int t;
	
	timer.interval = interval;

	t = (u32)((interval * 1190) / 1000);	

	/* First generator, operation - CE write, work mode 2, binary counting */
	hal_outb((void *)0x43, 0x34);
	
	/* Set counter */
	hal_outb((void *)0x40, (u8)(t & 0xff));
	hal_outb((void *)0x40, (u8)(t >> 8));
	
	return;
}
