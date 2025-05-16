/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2022 Phoenix Systems
 * Author: Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "config.h"
#include "hal/timer.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/string.h"


/* nrf9160 timer module provides instances from 0 to 2 */
#ifndef KERNEL_TIMER_INSTANCE
#define KERNEL_TIMER_INSTANCE 0
#endif


static struct {
	volatile u32 *timer[3];
	intr_handler_t overflowh;
	spinlock_t sp;
	volatile time_t timeUs;
	volatile u32 *lptim;
	volatile time_t upper;
	volatile int wakeup;
	u32 interval;
} timer_common;


/* clang-format off */
enum { timer_tasks_start = 0, timer_tasks_stop, timer_tasks_count, timer_tasks_clear, timer_tasks_shutdown,
	timer_tasks_capture0 = 16, timer_tasks_capture1, timer_tasks_capture2, timer_tasks_capture3, timer_tasks_capture4, timer_tasks_capture5,
	timer_events_compare0 = 80, timer_events_compare1, timer_events_compare2, timer_events_compare3, timer_events_compare4, timer_events_compare5,
	timer_intenset = 193, timer_intenclr, timer_mode = 321, timer_bitmode, timer_prescaler = 324,
	timer_cc0 = 336, timer_cc1, timer_cc2, timer_cc3, timer_cc4, timer_cc5 };
/* clang-format on */

static int timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)ctx;
	(void)arg;
	int ret = 0;

	/* Clear compare event */
	*(timer_common.timer[KERNEL_TIMER_INSTANCE] + timer_events_compare0) = 0U;
	/* Clear counter */
	*(timer_common.timer[KERNEL_TIMER_INSTANCE] + timer_tasks_clear) = 1U;

	timer_common.timeUs += timer_common.interval;
	hal_cpuDataSyncBarrier();

	return ret;
}


/* Interface functions */


void hal_timerSetWakeup(u32 waitUs)
{
}


time_t hal_timerGetUs(void)
{
	return timer_common.timeUs;
}


int hal_timerRegister(intrFn_t f, void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = TIMER_IRQ_ID;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


char *hal_timerFeatures(char *features, size_t len)
{
	hal_strncpy(features, "Using SysTick timer", len);
	features[len - 1] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
	/* using nrf9160 timer module */
	timer_common.timer[0] = (void *)0x5000f000U;
	timer_common.timer[1] = (void *)0x50010000U;
	timer_common.timer[2] = (void *)0x50011000U;
	timer_common.upper = 0;
	timer_common.wakeup = 0;
	timer_common.timeUs = 0;
	timer_common.interval = interval;

	hal_spinlockCreate(&timer_common.sp, "timer");

	/* Set timer mode */
	*(timer_common.timer[KERNEL_TIMER_INSTANCE] + timer_mode) = 0U;
	/* Set 16-bit mode */
	*(timer_common.timer[KERNEL_TIMER_INSTANCE] + timer_bitmode) = 0U;
	/* 1 tick per 1 us */
	*(timer_common.timer[KERNEL_TIMER_INSTANCE] + timer_prescaler) = 4U;
	/* 1 compare event per interval * 1us */
	*(timer_common.timer[KERNEL_TIMER_INSTANCE] + timer_cc0) = interval;
	/* Enable interrupts from compare0 events */
	*(timer_common.timer[KERNEL_TIMER_INSTANCE] + timer_intenset) = 0x10000U;

	/* Clear and start timer0 */
	*(timer_common.timer[KERNEL_TIMER_INSTANCE] + timer_tasks_clear) = 1U;
	*(timer_common.timer[KERNEL_TIMER_INSTANCE] + timer_tasks_start) = 1U;

	timer_common.overflowh.f = timer_irqHandler;
	/* irq number always equals nrf peripheral id + 16 */
	timer_common.overflowh.n = (unsigned int)timer0_irq + 16U;
	timer_common.overflowh.got = NULL;
	timer_common.overflowh.data = NULL;
	hal_interruptsSetHandler(&timer_common.overflowh);
	_nrf91_systickInit(interval);
}
