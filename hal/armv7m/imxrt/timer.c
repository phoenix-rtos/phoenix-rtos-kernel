/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Author: Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../../../timer.h"
#include "config.h"
#include <arch/cpu.h>
#include <arch/interrupts.h>


static struct {
	intr_handler_t handler;
	volatile time_t jiffies;
	spinlock_t sp;

	u32 interval;
} timer_common;


static int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	timer_common.jiffies += timer_common.interval;
	return 0;
}


void hal_timerSetWakeup(u32 when)
{
}


time_t hal_timerUs2Cyc(time_t us)
{
	return us;
}


time_t hal_timerCyc2Us(time_t cyc)
{
	return cyc;
}


time_t hal_timerGetUs(void)
{
	spinlock_ctx_t sc;
	time_t ret;

	hal_spinlockSet(&timer_common.sp, &sc);
	ret = hal_timerCyc2Us(timer_common.jiffies);
	hal_spinlockClear(&timer_common.sp, &sc);

	return ret;
}


time_t hal_timerGetCyc(void)
{
	spinlock_ctx_t sc;
	time_t ret;

	hal_spinlockSet(&timer_common.sp, &sc);
	ret = timer_common.jiffies;
	hal_spinlockClear(&timer_common.sp, &sc);

	return ret;
}

int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = SYSTICK_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


void _hal_timerInit(u32 interval)
{
	timer_common.jiffies = 0;

	_imxrt_systickInit(interval);

	timer_common.interval = interval;
	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = SYSTICK_IRQ;
	timer_common.handler.data = NULL;
	hal_interruptsSetHandler(&timer_common.handler);
}
