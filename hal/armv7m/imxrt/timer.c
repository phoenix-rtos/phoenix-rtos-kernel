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
#include "hal/armv7m/armv7m.h"
#include "config.h"
#include <arch/cpu.h>
#include <arch/interrupts.h>


enum { gpt_cr = 0, gpt_pr, gpt_sr, gpt_ir, gpt_ocr1, gpt_ocr2, gpt_ocr3, gpt_icr1, gpt_icr2, gpt_cnt };


static struct {
	intr_handler_t handler;
	volatile u32 upper;
	spinlock_t sp;

	volatile u32 *base;
	u32 interval;
} timer_common;


static time_t hal_timerCyc2Us(time_t ticks)
{
	return (ticks * 1024) / ((GPT_FREQ_MHZ * 1024) / (GPT_PRESCALER * GPT_OSC_PRESCALER));
}


static time_t hal_timerUs2Cyc(time_t us)
{
	return (((GPT_FREQ_MHZ * 1024) / (GPT_PRESCALER * GPT_OSC_PRESCALER)) * us + 512) / 1024;
}


static int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	if ((*(timer_common.base + gpt_sr) & (1 << 5)) != 0) { /* Roll-over */
		++timer_common.upper;
		*(timer_common.base + gpt_sr) |= (1 << 5);
	}

	if ((*(timer_common.base + gpt_sr) & (1 << 1)) != 0) { /* Compare match ch2 */
		*(timer_common.base + gpt_ocr2) += hal_timerUs2Cyc(timer_common.interval);
		hal_cpuDataMemoryBarrier();
		*(timer_common.base + gpt_sr) |= 1 << 1;
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

	if ((*(timer_common.base + gpt_sr) & (1 << 5)) != 0) {
		lower = *(timer_common.base + gpt_cnt);
		if (lower != 0xffffffffUL) {
			++upper;
		}
	}
	hal_spinlockClear(&timer_common.sp, &sc);

	return ((time_t)upper << 32) | lower;
}


void hal_timerSetWakeup(u32 waitUs)
{
	spinlock_ctx_t sc;

	if (waitUs > timer_common.interval) {
		waitUs = timer_common.interval;
	}

	hal_spinlockSet(&timer_common.sp, &sc);
	/* Modulo handled implicitly */
	*(timer_common.base + gpt_ocr2) = hal_timerUs2Cyc(waitUs) + *(timer_common.base + gpt_cnt);
	*(timer_common.base + gpt_sr) |= 1 << 1;
	hal_cpuDataMemoryBarrier();
	hal_spinlockClear(&timer_common.sp, &sc);
}


time_t hal_timerGetUs(void)
{
	return hal_timerCyc2Us(hal_timerGetCyc());
}


int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = GPT_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


void _hal_timerInit(u32 interval)
{
	timer_common.base = (void *)GPT_BASE;

	/* Disable timer */
	*(timer_common.base + gpt_cr) &= ~1u;
	hal_cpuDataMemoryBarrier();

	timer_common.interval = interval;
	timer_common.upper = 0;
	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = GPT_IRQ;
	timer_common.handler.data = NULL;
	hal_interruptsSetHandler(&timer_common.handler);

	/* Reset */
	*(timer_common.base + gpt_cr) |= 1 << 15;
	hal_cpuDataMemoryBarrier();
	while ((*(timer_common.base + gpt_cr) & (1 << 15)) != 0) {
	}

	/* Set prescaler, prescale OSC by GPT_OSC_PRESCALER to get less than 1/4 bus clk */
	*(timer_common.base + gpt_pr) = ((GPT_OSC_PRESCALER - 1) << 12) | (GPT_PRESCALER - 1);

	/* Enable oscillator input and select it as clock source, freerun mode */
	/* Leave timer running in lp modes, reset counter on enable */
	*(timer_common.base + gpt_cr) = (1 << 10) | (1 << 9) | (5 << 6) | (1 << 5) | (1 << 4) | (1 << 3) | (1 << 1);
	hal_cpuDataMemoryBarrier();

	/* Enable */
	*(timer_common.base + gpt_cr) |= 1;
	hal_cpuDataMemoryBarrier();

	/* Enable roll-over and ocr2 interrupts */
	*(timer_common.base + gpt_ir) = (1 << 5) | (1 << 1);
}
