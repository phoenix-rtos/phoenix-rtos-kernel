/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver based on STM32 TIM peripheral.
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

/* This implementation can use timers described as "basic" in the documentation.
 * A more advanced timer can be used (like "general-purpose" or "advanced-control" timers),
 * but a "basic" has enough functionality for our needs.
 */

/* List of registers cut down to only those available on basic timers */
enum tim_regs {
	tim_cr1 = 0U,
	tim_cr2,
	tim_dier = 3U,
	tim_sr,
	tim_egr,
	tim_cnt = 9U,
	tim_psc,
	tim_arr,
};

static struct {
	intr_handler_t handler;
	volatile u64 ticks;
	spinlock_t sp;
	volatile u32 *base;

	u32 frequency;        /* Timer ticks per second */
	u32 ticksPerInterval; /* Timer ticks per interval (i.e. between timer interrupts) */
} timer_common;


int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	/* Note: hal_getTicks() may have cleared the interrupt flag and added to the tick count,
	 * but after clearing the flag the interrupt remains pending. That's why we need to check
	 * SR to make sure we don't add twice for the same update event. */
	if ((*(timer_common.base + tim_sr) & 1U) != 0) {
		*(timer_common.base + tim_sr) = ~1U; /* Flags are write 0 to clear */
		timer_common.ticks += timer_common.ticksPerInterval;
	}

	return 0;
}


void timer_jiffiesAdd(time_t t)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.sp, &sc);
	if (timer_common.frequency == 1000000UL) {
		timer_common.ticks += t;
	}
	else {
		timer_common.ticks += (t * timer_common.frequency) / 1000000UL;
	}
	hal_spinlockClear(&timer_common.sp, &sc);
}


char *hal_timerFeatures(char *features, size_t len)
{
	hal_strncpy(features, "Using STM32 TIM timer", len);
	features[len - 1] = '\0';
	return features;
}


static u64 hal_getTicks(void)
{
	spinlock_ctx_t sc;
	u32 cntval;
	u64 ret;

	hal_spinlockSet(&timer_common.sp, &sc);
	ret = timer_common.ticks;
	cntval = *(timer_common.base + tim_cnt);
	if ((cntval >> 31) != 0) {
		*(timer_common.base + tim_sr) = ~1U;
		ret += timer_common.ticksPerInterval;
		timer_common.ticks = ret;
	}

	ret += cntval & 0xffffU;
	hal_spinlockClear(&timer_common.sp, &sc);

	return ret;
}


time_t hal_timerGetUs(void)
{
	u64 ticks = hal_getTicks();
	if (timer_common.frequency == 1000000UL) {
		return (time_t)ticks;
	}
	else {
		return ((time_t)ticks * 1000000) / timer_common.frequency;
	}
}


int hal_timerRegister(intrFn_t f, void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = TIM_SYSTEM_IRQ;
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
	u32 prescaler;
	timer_common.ticks = 0;
	timer_common.frequency = TIM_SYSTEM_FREQ;
	if ((timer_common.frequency % 1000000UL) == 0) {
		/* If frequency divisible by 1 MHz, set the prescaler to tick once per microsecond.
		 * Timer APIs work on microseconds, so in this mode we avoid having to do 64-bit division
		 * in hal_timerGetUs (a very frequently called function). */
		prescaler = timer_common.frequency / 1000000UL;
		timer_common.frequency = 1000000UL;
		timer_common.ticksPerInterval = interval;
		LIB_ASSERT((prescaler >= 1) && (prescaler <= 65535), "Selected timer interval is not achievable");
	}
	else {
		timer_common.ticksPerInterval = ((u64)timer_common.frequency * interval) / 1000000UL;
		/* TODO: For optimal precision prescaler should be a factor of timer_common.ticksPerInterval,
		 * but the difference in precision isn't big enough to matter, so I don't want to add a whole lot
		 * of extra code to handle this. */
		prescaler = (timer_common.ticksPerInterval + 65535) / 65536;
		LIB_ASSERT((prescaler >= 1) && (prescaler <= 65535), "Selected timer interval is not achievable");
		timer_common.frequency /= prescaler;
		timer_common.ticksPerInterval = ((u64)timer_common.frequency * interval) / 1000000UL;
	}

	LIB_ASSERT((timer_common.ticksPerInterval >= 1) && (timer_common.ticksPerInterval <= 65535),
			"Selected timer interval is not achievable");
	_stm32_rccSetDevClock(TIM_SYSTEM_PCTL, 1, 1);
	_stm32_dbgmcuStopTimerInDebug(TIM_SYSTEM_PCTL, 1);
	timer_common.base = TIM_SYSTEM_BASE;
	/* set UIF status bit remapping, so we can get UIF by just reading the counter */
	*(timer_common.base + tim_cr1) = (1 << 11);
	*(timer_common.base + tim_cr2) = 0;
	*(timer_common.base + tim_cnt) = 0;
	*(timer_common.base + tim_psc) = prescaler - 1;
	*(timer_common.base + tim_arr) = timer_common.ticksPerInterval - 1;
	*(timer_common.base + tim_dier) = 1; /* Activate interrupt */

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = TIM_SYSTEM_IRQ;
	timer_common.handler.data = NULL;
	hal_interruptsSetHandler(&timer_common.handler);

	hal_cpuDataMemoryBarrier();
	*(timer_common.base + tim_cr1) |= 1; /* Start counting */
}
