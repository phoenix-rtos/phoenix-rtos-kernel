/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver (HAL RISCV64)
 *
 * Copyright 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/timer.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "riscv64.h"
#include "sbi.h"



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
	cycles_t c = hal_cpuGetCycles2() / 1000;

	timer.interval = interval;
	timer.jiffies = 0;

	sbi_ecall(SBI_SETTIMER, 0, c + 1000L, 0, 0, 0, 0, 0);
	csr_set(sie, SIE_STIE);

	hal_spinlockCreate(&timer.sp, "timer");
	timer.handler.f = timer_irqHandler;
	timer.handler.n = SYSTICK_IRQ;
	timer.handler.data = NULL;
	hal_interruptsSetHandler(&timer.handler);

	return;
}
