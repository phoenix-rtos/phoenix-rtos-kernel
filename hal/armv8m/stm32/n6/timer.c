/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver based on SysTick
 *
 * Copyright 2012, 2017, 2021, 2025 Phoenix Systems
 * Author: Jakub Sejdak, Aleksander Kaminski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/armv8m/stm32/n6/config.h"
#include "hal/timer.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "lib/assert.h"

static struct {
	intr_handler_t handler;
	volatile u64 ticks;
	spinlock_t sp;

	u32 frequency;        /* Timer ticks per second */
	u32 ticksPerInterval; /* Timer ticks per interval (i.e. between timer interrupts) */
} timer_common;


int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;
	u8 hasOverflowed;

	/* We need to check the overflow flag, because hal_getTicks() may have cleared it */
	_hal_scsSystickGetCount(&hasOverflowed);
	if (hasOverflowed) {
		timer_common.ticks += timer_common.ticksPerInterval;
	}

	return 0;
}


void timer_jiffiesAdd(time_t t)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.sp, &sc);
	timer_common.ticks += (t * timer_common.frequency) / 1000000;
	hal_spinlockClear(&timer_common.sp, &sc);
}


char *hal_timerFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using SysTick timer", len);
	features[len - 1] = '\0';
	return features;
}


static u64 hal_getTicks(void)
{
	spinlock_ctx_t sc;
	u8 hasOverflowed;
	u32 elapsed;
	u64 ret;

	hal_spinlockSet(&timer_common.sp, &sc);
	elapsed = timer_common.ticksPerInterval - _hal_scsSystickGetCount(&hasOverflowed);
	if (hasOverflowed != 0) {
		/* Reading the flag cleared the overflow, so we need to add the value to the counter */
		timer_common.ticks += timer_common.ticksPerInterval;
	}

	ret = timer_common.ticks + elapsed;
	hal_spinlockClear(&timer_common.sp, &sc);

	return ret;
}


time_t hal_timerGetUs(void)
{
	return (hal_getTicks() * 1000000) / timer_common.frequency;
}


int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = SYSTICK_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


void hal_timerSetWakeup(u32 waitUs)
{
	/* Not implemented yet */
}


/* interval: microseconds between timer interrupts */
void _hal_timerInit(u32 interval)
{
	timer_common.ticks = 0;
	timer_common.frequency = _stm32_rccGetCPUClock();
	timer_common.ticksPerInterval = (((u64)timer_common.frequency) * interval) / 1000000;
	LIB_ASSERT(timer_common.ticksPerInterval <= 0x01000000, "Selected timer interval is not achievable");
	_hal_scsSystickInit(timer_common.ticksPerInterval - 1);
	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = SYSTICK_IRQ;
	timer_common.handler.data = NULL;
	hal_interruptsSetHandler(&timer_common.handler);
}
