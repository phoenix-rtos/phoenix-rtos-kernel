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
#include "hal/string.h"

#include <arch/timer.h>

#include <board_config.h>


static struct {
	intr_handler_t handler;
	spinlock_t sp;

	u64 interval;
} timer_common;


static int timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	hal_sbiSetTimer(csr_read(time) + timer_common.interval);

	return 0;
}


void hal_timerSetWakeup(u32 waitUs)
{
	hal_sbiSetTimer(csr_read(time) + waitUs * ((u64)TIMER_FREQ / (1000UL * 1000UL)));
}


time_t hal_timerGetUs(void)
{
	return (time_t)csr_read(time) / ((time_t)TIMER_FREQ / (1000 * 1000));
}


int hal_timerRegister(intrFn_t f, void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = (unsigned int)SYSTICK_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


char *hal_timerFeatures(char *features, size_t len)
{
	if (len != 0U) {
		(void)hal_strncpy(features, "Using hypervisor timer", len);
		features[len - 1U] = '\0';
	}
	return features;
}


void hal_timerInitCore(void)
{
	hal_sbiSetTimer(csr_read(time) + timer_common.interval);
}


__attribute__((section(".init"))) void _hal_timerInit(u32 interval)
{
	timer_common.interval = interval * ((unsigned int)TIMER_FREQ / 1000000UL);

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = timer_irqHandler;
	timer_common.handler.n = (unsigned int)SYSTICK_IRQ;
	timer_common.handler.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.handler);

	hal_sbiSetTimer(csr_read(time) + timer_common.interval);
}
