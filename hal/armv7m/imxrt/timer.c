/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2012, 2017, 2022 Phoenix Systems
 * Author: Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/timer.h"
#include "config.h"
#include <arch/cpu.h>
#include <arch/interrupts.h>
#include "hal/cpu.h"
#include "hal/string.h"


enum { gpt_cr = 0,
	gpt_pr,
	gpt_sr,
	gpt_ir,
	gpt_ocr1,
	gpt_ocr2,
	gpt_ocr3,
	gpt_icr1,
	gpt_icr2,
	gpt_cnt };


static struct {
	intr_handler_t handler;
	volatile u32 upper;
	spinlock_t sp;

	volatile u32 *base;
	u32 interval;
} timer_common;


static time_t hal_timerCyc2us(time_t ticks)
{
	return (ticks * 1024) / ((GPT_FREQ_MHZ * 1024) / (GPT_PRESCALER * GPT_OSC_PRESCALER));
}


static time_t hal_timerUs2Cyc(time_t us)
{
	return (((GPT_FREQ_MHZ * 1024) / (GPT_PRESCALER * GPT_OSC_PRESCALER)) * us + 512) / 1024;
}


static int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	if ((*(timer_common.base + gpt_sr) & (1U << 5)) != 0U) { /* Roll-over */
		++timer_common.upper;
		*(timer_common.base + gpt_sr) = (1U << 5);
	}

	if ((*(timer_common.base + gpt_sr) & (1U << 1)) != 0U) { /* Compare match ch2 */
		*(timer_common.base + gpt_ocr2) += (u32)hal_timerUs2Cyc((time_t)timer_common.interval);
		hal_cpuDataMemoryBarrier();
		*(timer_common.base + gpt_sr) = (1U << 1);
	}

	hal_cpuDataMemoryBarrier();

	return 0;
}


static time_t hal_timerGetCyc(void)
{
	u32 upper, lower;
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.sp, &sc);
	upper = timer_common.upper;
	lower = *(timer_common.base + gpt_cnt);

	if ((*(timer_common.base + gpt_sr) & (1U << 5)) != 0U) {
		lower = *(timer_common.base + gpt_cnt);
		if (lower != 0xffffffffUL) {
			++upper;
		}
	}
	hal_spinlockClear(&timer_common.sp, &sc);

	return (time_t)(u64)(((u64)upper << 32) | lower);
}


void hal_timerSetWakeup(u32 waitUs)
{
	spinlock_ctx_t sc;

	if (waitUs > timer_common.interval) {
		waitUs = timer_common.interval;
	}

	hal_spinlockSet(&timer_common.sp, &sc);
	/* Modulo handled implicitly */
	*(timer_common.base + gpt_ocr2) = (u32)hal_timerUs2Cyc((time_t)waitUs) + *(timer_common.base + gpt_cnt);
	*(timer_common.base + gpt_sr) = (1UL << 1);
	hal_cpuDataMemoryBarrier();
	hal_spinlockClear(&timer_common.sp, &sc);
}


time_t hal_timerGetUs(void)
{
	return hal_timerCyc2us(hal_timerGetCyc());
}


int hal_timerRegister(intrFn_t f, void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = GPT_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


char *hal_timerFeatures(char *features, size_t len)
{
	(void)hal_strncpy(features, "Using General Purpose Timer", len);
	features[len - 1U] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
	timer_common.base = (void *)GPT_BASE;

	/* Disable timer */
	*(timer_common.base + gpt_cr) &= ~1U;
	hal_cpuDataMemoryBarrier();

	timer_common.interval = interval;
	timer_common.upper = 0;
	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = GPT_IRQ;
	timer_common.handler.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.handler);

	/* Reset */
	*(timer_common.base + gpt_cr) |= 1UL << 15;
	hal_cpuDataMemoryBarrier();
	while ((*(timer_common.base + gpt_cr) & (1UL << 15)) != 0U) {
	}

	/* Set prescaler, prescale OSC by GPT_OSC_PRESCALER to get less than 1/4 bus clk */
	*(timer_common.base + gpt_pr) = (((u32)GPT_OSC_PRESCALER - 1UL) << 12) | ((u32)GPT_PRESCALER - 1U);

	/* Enable oscillator input and select it as clock source, freerun mode */
	/* Leave timer running in lp modes, reset counter on enable */
	*(timer_common.base + gpt_cr) = (1UL << 10) | (1UL << 9) | (5UL << 6) | (1UL << 5) | (1UL << 4) | (1UL << 3) | (1UL << 1);
	hal_cpuDataMemoryBarrier();

	/* Enable */
	*(timer_common.base + gpt_cr) |= 1UL;
	hal_cpuDataMemoryBarrier();

	/* Enable roll-over and ocr2 interrupts */
	*(timer_common.base + gpt_ir) = (1UL << 5) | (1UL << 1);
}
