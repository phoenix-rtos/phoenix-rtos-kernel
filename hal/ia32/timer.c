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
#include "spinlock.h"

#include "../../include/errno.h"


struct {
	intr_handler_t handler;
	volatile time_t jiffies;
	spinlock_t sp;

	u32 interval;
} timer;


static int timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	timer.jiffies += timer.interval;
	return 0;
}


time_t hal_getTimer(void)
{
	spinlock_ctx_t sc;
	time_t ret;

	hal_spinlockSet(&timer.sp, &sc);
	ret = timer.jiffies;
	hal_spinlockClear(&timer.sp, &sc);

	return ret;
}


int timer_reschedule(unsigned int n, cpu_context_t *ctx, void *arg)
{
//	timer.scheduler(ctx);
	return EOK;
}


__attribute__ ((section (".init"))) void _timer_init(u32 interval)
{
	unsigned int t;

	timer.interval = interval;
	timer.jiffies = 0;

	t = (u32)((interval * 1190) / 1000);

	/* First generator, operation - CE write, work mode 2, binary counting */
	hal_outb((void *)0x43, 0x34);

	/* Set counter */
	hal_outb((void *)0x40, (u8)(t & 0xff));
	hal_outb((void *)0x40, (u8)(t >> 8));

	hal_spinlockCreate(&timer.sp, "timer");
	timer.handler.f = timer_irqHandler;
	timer.handler.n = SYSTICK_IRQ;
	timer.handler.data = NULL;
	hal_interruptsSetHandler(&timer.handler);

	return;
}
