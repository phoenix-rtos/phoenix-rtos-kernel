/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2012, 2017, 2021 Phoenix Systems
 * Author: Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "cpu.h"
#include "stm32.h"
#include "interrupts.h"
#include "spinlock.h"

static struct {
	intr_handler_t handler;
	volatile time_t jiffies;
	spinlock_t sp;

	u32 interval;
} timer_common;


int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	timer_common.jiffies += timer_common.interval;
	return -1;
}


void timer_jiffiesAdd(time_t t)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.sp, &sc);
	timer_common.jiffies += t;
	hal_spinlockClear(&timer_common.sp, &sc);
}


time_t hal_getTimer(void)
{
	spinlock_ctx_t sc;
	time_t ret;

	hal_spinlockSet(&timer_common.sp, &sc);
	ret = timer_common.jiffies;
	hal_spinlockClear(&timer_common.sp, &sc);

	return ret;
}


void _timer_init(u32 interval)
{
	_stm32_systickInit(interval);

	timer_common.interval = interval;
	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = SYSTICK_IRQ;
	timer_common.handler.data = NULL;
	hal_interruptsSetHandler(&timer_common.handler);
}
