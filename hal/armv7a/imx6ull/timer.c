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

#include "hal/armv7a/armv7a.h"
#include "hal/timer.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "config.h"

static struct {
	volatile u32 *epit1;
	volatile u32 *gpt1;

	volatile u32 timerhi;

	intr_handler_t wakeuph;
	intr_handler_t timerh;
	spinlock_t lock;
} timer_common;


/* clang-format off */
enum { epit_cr = 0, epit_sr, epit_lr, epit_cmpr, epit_cnr };


enum { gpt_cr = 0, gpt_pr, gpt_sr, gpt_ir, gpt_ocr1, gpt_ocr2, gpt_ocr3, gpt_icr1, gpt_icr2, gpt_cnt };
/* clang-format on */


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Provided by toolchain" */
extern unsigned int _end;


static int timer_wakeupIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	/* Clear irq flag and turn off timer */
	/* Will be turned on again in the threads_timeitr via hal_setWakeup */

	*(timer_common.epit1 + epit_cr) &= ~1U;
	*(timer_common.epit1 + epit_sr) = 1;
	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();

	return 0;
}


static int timer_overflowIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	/* Handle GPT1 wrap-around */
	*(timer_common.gpt1 + gpt_sr) |= 1UL << 5;
	++timer_common.timerhi;

	return 0;
}


static time_t hal_timerCyc2us(time_t cyc)
{
	return cyc / 66LL;
}


static time_t hal_timerGetCyc(void)
{
	u32 reg;
	unsigned long long ret;
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.lock, &sc);
	reg = *(timer_common.gpt1 + gpt_cnt);
	ret = ((u64)timer_common.timerhi << 32) | reg;
	if (((*(timer_common.gpt1 + gpt_sr) & (1UL << 5)) != 0U) && ((reg & (1UL << 31)) == 0U)) {
		ret += 1ULL << 32;
	}
	hal_spinlockClear(&timer_common.lock, &sc);

	return (time_t)ret;
}


void hal_timerSetWakeup(u32 waitUs)
{
	spinlock_ctx_t sc;
	u32 cyc;

	if (waitUs == 0U) {
		++waitUs;
	}

	cyc = waitUs * 66U;

	hal_spinlockSet(&timer_common.lock, &sc);
	*(timer_common.epit1 + epit_lr) = cyc;
	*(timer_common.epit1 + epit_cr) |= 1U;
	hal_spinlockClear(&timer_common.lock, &sc);
}


time_t hal_timerGetUs(void)
{
	time_t ret = hal_timerGetCyc();

	return hal_timerCyc2us(ret);
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
	(void)hal_strncpy(features, "Using EPIT and GPT timers", len);
	features[len - 1U] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
	timer_common.epit1 = (void *)(((u32)&_end + (9U * SIZE_PAGE) - 1U) & ~(SIZE_PAGE - 1U));
	timer_common.gpt1 = (void *)(((u32)&_end + (10U * SIZE_PAGE) - 1U) & ~(SIZE_PAGE - 1U));
	timer_common.timerhi = 0;

	hal_spinlockCreate(&timer_common.lock, "timer");

	timer_common.wakeuph.data = NULL;
	timer_common.wakeuph.n = TIMER_IRQ_ID;
	timer_common.wakeuph.f = timer_wakeupIrqHandler;

	(void)hal_interruptsSetHandler(&timer_common.wakeuph);

	timer_common.timerh.data = NULL;
	timer_common.timerh.n = 87;
	timer_common.timerh.f = timer_overflowIrqHandler;

	(void)hal_interruptsSetHandler(&timer_common.timerh);

	/* Input clock 66 MHz, prescaler for both timers is set to 1 */

	*(timer_common.epit1 + epit_cr) |= 1UL << 16;
	while ((*(timer_common.epit1 + epit_cr) & (1UL << 16)) != 0U) {
		;
	}
	*(timer_common.epit1 + epit_cmpr) = 0;
	*(timer_common.epit1 + epit_cr) |= (1UL << 17) | 1U;
	*(timer_common.epit1 + epit_lr) = (1000U * 1000U) / interval;
	*(timer_common.epit1 + epit_cr) &= ~1U;
	*(timer_common.epit1 + epit_cr) = 0x016a000e;
	*(timer_common.epit1 + epit_cr) |= 1U;

	*(timer_common.gpt1 + gpt_cr) &= ~1U;
	*(timer_common.gpt1 + gpt_ir) &= ~0x3fU;
	*(timer_common.gpt1 + gpt_pr) = 0x00U;
	*(timer_common.gpt1 + gpt_sr) = 0x1fU;
	*(timer_common.gpt1 + gpt_ir) |= 1U << 5;
	*(timer_common.gpt1 + gpt_cr) = (1UL << 9) | (1U << 6) | (0x7U << 3);
	*(timer_common.gpt1 + gpt_cr) |= 1U;
}
