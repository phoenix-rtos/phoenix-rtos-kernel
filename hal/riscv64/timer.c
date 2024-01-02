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
#include "hal/console.h"

#include <board_config.h>

static struct {
	intr_handler_t handler;
	volatile time_t jiffies;
	spinlock_t sp;

	u64 interval;
} timer_common;


static int timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	++timer_common.jiffies;
	hal_sbiSetTimer(csr_read(time) + timer_common.interval);

	return 0;
}


void hal_timerSetWakeup(u32 waitUs)
{
}


time_t hal_timerGetUs(void)
{
	spinlock_ctx_t sc;
	time_t ret;

	hal_spinlockSet(&timer_common.sp, &sc);
	ret = timer_common.jiffies;
	hal_spinlockClear(&timer_common.sp, &sc);

	return ret * 1000ULL;
}


int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = SYSTICK_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


char *hal_timerFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using hypervisor timer", len);
	features[len - 1] = '\0';
	return features;
}


__attribute__((section(".init"))) void _hal_timerInit(u32 interval)
{
	timer_common.interval = interval * (TIMER_FREQ / 1000000ULL);
	timer_common.jiffies = 0;

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = timer_irqHandler;
	timer_common.handler.n = SYSTICK_IRQ;
	timer_common.handler.data = NULL;
	hal_interruptsSetHandler(&timer_common.handler);

	hal_sbiSetTimer(csr_read(time) + timer_common.interval);
}
