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

#include "hal/timer.h"
#include "hal/cpu.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "ia32.h"

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

void hal_timerSetWakeup(u32 waitUs)
{
}


time_t hal_timerGetUs(void)
{
	spinlock_ctx_t sc;
	time_t ret;

	hal_spinlockSet(&timer.sp, &sc);
	ret = timer.jiffies;
	hal_spinlockClear(&timer.sp, &sc);

	return ret;
}


int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = SYSTICK_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


__attribute__((section(".init"))) void _hal_timerInit(u32 interval)
{
	unsigned int t;

	interval /= hal_cpuGetCount();

	timer.interval = interval;
	timer.jiffies = 0;

	t = (u32)((interval * 1190) / 1000);

	/* First generator, operation - CE write, work mode 2, binary counting */
	hal_outb(PORT_PIT_COMMAND, 0x34);

	/* Set counter */
	hal_outb(PORT_PIT_DATA_CHANNEL0, (u8)(t & 0xff));
	hal_outb(PORT_PIT_DATA_CHANNEL0, (u8)(t >> 8));

	hal_spinlockCreate(&timer.sp, "timer");
	timer.handler.f = timer_irqHandler;
	timer.handler.n = SYSTICK_IRQ;
	timer.handler.data = NULL;
	hal_interruptsSetHandler(&timer.handler);

	return;
}
