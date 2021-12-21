/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2018 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../armv7a.h"
#include "../../timer.h"
#include "../../spinlock.h"

#define TIMER_IRQ_ID 88

struct {
	volatile u32 *epit1;
	volatile u32 *gpt1;

	volatile u32 timerhi;

	intr_handler_t wakeuph;
	intr_handler_t timerh;
	spinlock_t lock;
} timer_common;


enum { epit_cr = 0, epit_sr, epit_lr, epit_cmpr, epit_cnr };


enum { gpt_cr = 0, gpt_pr, gpt_sr, gpt_ir, gpt_ocr1, gpt_ocr2, gpt_ocr3, gpt_icr1, gpt_icr2, gpt_cnt };


extern unsigned int _end;


static int timer_wakeupIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	/* Clear irq flag and turn off timer */
	/* Will be turned on again in the threads_timeitr via hal_setWakeup */

	*(timer_common.epit1 + epit_cr) &= ~1;
	*(timer_common.epit1 + epit_sr) = 1;
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();

	return 0;
}


static int timer_overflowIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	/* Handle GPT1 wrap-around */
	*(timer_common.gpt1 + gpt_sr) |= 1 << 5;
	++timer_common.timerhi;

	return 0;
}


static time_t hal_timerCyc2Us(time_t cyc)
{
	return cyc / 66LL;
}


static time_t hal_timerGetCyc(void)
{
	u32 reg;
	time_t ret;
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.lock, &sc);
	reg = *(timer_common.gpt1 + gpt_cnt);
	ret = ((time_t)timer_common.timerhi << 32) | (time_t)reg;
	if ((*(timer_common.gpt1 + gpt_sr) & (1 << 5)) && !(reg & (1 << 31)))
		ret += 1ULL << 32;
	hal_spinlockClear(&timer_common.lock, &sc);

	return ret;
}


void hal_timerSetWakeup(u32 when)
{
	spinlock_ctx_t sc;

	if (!when)
		++when;

	hal_spinlockSet(&timer_common.lock, &sc);
	*(timer_common.epit1 + epit_lr) = when;
	*(timer_common.epit1 + epit_cr) |= 1;
	hal_spinlockClear(&timer_common.lock, &sc);
}


time_t hal_timerGetUs(void)
{
	time_t ret = hal_timerGetCyc();

	return hal_timerCyc2Us(ret);
}


int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = TIMER_IRQ_ID;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


void _hal_timerInit(u32 interval)
{
	timer_common.epit1 = (void *)(((u32)&_end + (9 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));
	timer_common.gpt1 = (void *)(((u32)&_end + (10 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));
	timer_common.timerhi = 0;

	hal_spinlockCreate(&timer_common.lock, "timer");

	timer_common.wakeuph.data = NULL;
	timer_common.wakeuph.n = TIMER_IRQ_ID;
	timer_common.wakeuph.f = timer_wakeupIrqHandler;

	hal_interruptsSetHandler(&timer_common.wakeuph);

	timer_common.timerh.data = NULL;
	timer_common.timerh.n = 87;
	timer_common.timerh.f = timer_overflowIrqHandler;

	hal_interruptsSetHandler(&timer_common.timerh);

	/* Input clock 66 MHz, prescaler for both timers is set to 1 */

	*(timer_common.epit1 + epit_cr) |= 1 << 16;
	while (*(timer_common.epit1 + epit_cr) & (1 << 16));
	*(timer_common.epit1 + epit_cmpr) = 0;
	*(timer_common.epit1 + epit_cr) |= (1 << 17) | 1;
	*(timer_common.epit1 + epit_lr) = (1000 * 1000) / interval;
	*(timer_common.epit1 + epit_cr) &= ~1;
	*(timer_common.epit1 + epit_cr) = 0x016a000e;
	*(timer_common.epit1 + epit_cr) |= 1;

	*(timer_common.gpt1 + gpt_cr) &= ~1;
	*(timer_common.gpt1 + gpt_ir) &= ~0x3f;
	*(timer_common.gpt1 + gpt_pr) = 0x00;
	*(timer_common.gpt1 + gpt_sr) = 0x1f;
	*(timer_common.gpt1 + gpt_ir) |= 1 << 5;
	*(timer_common.gpt1 + gpt_cr) = (1 << 9) | (1 << 6) | (0x7 << 3);
	*(timer_common.gpt1 + gpt_cr) |= 1;
}
