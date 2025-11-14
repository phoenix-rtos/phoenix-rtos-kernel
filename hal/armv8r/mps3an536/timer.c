/*
 * Phoenix-RTOS
 *
 * Operating system loader
 *
 * ARM Dual Timer driver
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "hal/hal.h"

#include "hal/armv8r/armv8r.h"

#include <board_config.h>

/* Timer registers */
/* clang-format off */
enum {
	timer1_load = 0, timer1_value, timer1_ctrl, timer1_intclr, timer1_ris, timer1_mis, timer1_bgload,
	timer2_load = 8, timer2_value, timer2_ctrl, timer2_intclr, timer2_ris, timer2_mis, timer2_bgload
};
/* clang-format on */


static struct {
	volatile u32 *base;
	volatile time_t time;
	u32 interval;
	intr_handler_t handler;
	spinlock_t lock;
} timer_common;


static int hal_timerIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	if ((*(timer_common.base + timer1_mis) & 0x1U) != 0U) {
		*(timer_common.base + timer1_intclr) = 0;
		timer_common.time++;
		hal_cpuDataSyncBarrier();
	}

	return 0;
}


static time_t hal_timerCyc2us(u32 cyc)
{
	return (time_t)cyc / (SYSCLK_FREQ / 1000 * 1000);
}


time_t hal_timerGetUs(void)
{
	spinlock_ctx_t sc;
	time_t jiffies;
	u32 cnt;

	hal_spinlockSet(&timer_common.lock, &sc);
	jiffies = timer_common.time;
	cnt = *(timer_common.base + timer1_value);

	/* Check if we have pending irq */
	if ((*(timer_common.base + timer1_mis) & 0x1U) != 0U) {
		jiffies++;
		cnt = *(timer_common.base + timer1_load);
		/* Don't update common time here, we'll still get the interrupt */
	}
	hal_spinlockClear(&timer_common.lock, &sc);

	cnt = *(timer_common.base + timer1_load) - cnt;

	return jiffies * 1000 + hal_timerCyc2us(cnt);
}


void hal_timerSetWakeup(u32 waitUs)
{
}


int hal_timerRegister(intrFn_t f, void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = (unsigned int)TIMER_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


char *hal_timerFeatures(char *features, size_t len)
{
	(void)hal_strncpy(features, "Using ARM Dual Timer", len);
	features[len - 1U] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
	timer_common.base = TIMER_BASE;
	timer_common.interval = interval;

	timer_common.handler.f = hal_timerIrqHandler;
	timer_common.handler.n = (unsigned int)TIMER_IRQ;
	timer_common.handler.data = NULL;

	hal_spinlockCreate(&timer_common.lock, "timer");

	/* Disable timer */
	*(timer_common.base + timer1_ctrl) &= ~(1U << 7);
	*(timer_common.base + timer1_value) = 0;

	/* Periodic mode, 32-bit, enable interrupt */
	*(timer_common.base + timer1_ctrl) = (1U << 6) | (1U << 5) | (1U << 1);
	hal_cpuDataSyncBarrier();
	*(timer_common.base + timer1_load) = ((u32)SYSCLK_FREQ / 1000U) - 1U;
	hal_cpuDataSyncBarrier();

	(void)hal_interruptsSetHandler(&timer_common.handler);

	/* Enable timer */
	*(timer_common.base + timer1_ctrl) |= (1U << 7);
}
