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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/armv7a/armv7a.h"
#include "hal/timer.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "hal/pmap.h"
#include "config.h"

static struct {
	volatile u32 *epit1;
	volatile u32 *gpt1;

	volatile u32 timerhi;

	intr_handler_t wakeuph;
	intr_handler_t timerh;
	spinlock_t lock;

	struct {
		u32 mult; /* delta * mult >> 32 == microseconds */
		u64 base;
		time_t baseUs;
		u8 usable;
	} genTmr;
} timer_common;


/* How long a delta may grow before the base is moved up, kept well below the
 * point where `delta * mult` would overflow */
#define GENTMR_REBASE (1ULL << 31)


/* clang-format off */
enum { epit_cr = 0, epit_sr, epit_lr, epit_cmpr, epit_cnr };


enum { gpt_cr = 0, gpt_pr, gpt_sr, gpt_ir, gpt_ocr1, gpt_ocr2, gpt_ocr3, gpt_icr1, gpt_icr2, gpt_cnt };
/* clang-format on */


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


/* Assumes timer_common.lock is held */
static time_t _hal_timerGetCyc(void)
{
	u32 reg;
	unsigned long long ret;

	reg = *(timer_common.gpt1 + gpt_cnt);
	ret = ((u64)timer_common.timerhi << 32) | reg;
	if (((*(timer_common.gpt1 + gpt_sr) & (1UL << 5)) != 0U) && ((reg & (1UL << 31)) == 0U)) {
		ret += 1ULL << 32;
	}

	return (time_t)ret;
}


/*
 * The GPT counter sits behind the peripheral bus, so reading it costs two
 * device accesses - and the scheduler wants a timestamp on every context
 * switch. The Cortex-A7 Generic Timer counts at a fixed rate in a register
 * instead, so it is used when it is there and running. Its rate is measured
 * against the GPT rather than taken from CNTFRQ, which firmware is free to
 * leave unset, and it is turned into a multiply so that reading the time needs
 * no division.
 */
static inline u64 _hal_genericTimerRead(void)
{
	u32 lo, hi;

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
	__asm__ volatile("mrrc p15, 0, %0, %1, c14" : "=r"(lo), "=r"(hi)); /* CNTPCT */

	return ((u64)hi << 32) | lo;
}


/* Assumes timer_common.lock is held */
static time_t _hal_genericTimerGetUs(void)
{
	u64 now = _hal_genericTimerRead();
	u64 delta = now - timer_common.genTmr.base;

	while (delta >= GENTMR_REBASE) {
		timer_common.genTmr.baseUs += (time_t)((GENTMR_REBASE * (u64)timer_common.genTmr.mult) >> 32);
		timer_common.genTmr.base += GENTMR_REBASE;
		delta -= GENTMR_REBASE;
	}

	return timer_common.genTmr.baseUs + (time_t)((delta * (u64)timer_common.genTmr.mult) >> 32);
}


/* Runs at the end of _hal_timerInit(), where nothing else can be asking for
 * the time yet, so it reads the GPT without taking the lock */
static int _hal_genericTimerInit(void)
{
	u32 pfr1;
	u64 c0, c1, ticks;
	time_t t0, t1, us;

	/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
	__asm__ volatile("mrc p15, 0, %0, c0, c1, 1" : "=r"(pfr1)); /* ID_PFR1 */
	if (((pfr1 >> 16) & 0xfU) == 0U) {
		/* Generic Timer not implemented */
		return -1;
	}

	t0 = hal_timerCyc2us(_hal_timerGetCyc());
	c0 = _hal_genericTimerRead();
	do {
		t1 = hal_timerCyc2us(_hal_timerGetCyc());
	} while ((t1 - t0) < 10000);
	c1 = _hal_genericTimerRead();

	ticks = c1 - c0;
	us = t1 - t0;

	/* A counter that does not run, or runs too slowly to tell microseconds apart */
	if ((ticks == 0ULL) || (us <= 0) || (ticks < (u64)us)) {
		return -1;
	}

	timer_common.genTmr.mult = (u32)((((u64)us) << 32) / ticks);
	timer_common.genTmr.base = c1;
	timer_common.genTmr.baseUs = t1;
	timer_common.genTmr.usable = 1;

	return 0;
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
	spinlock_ctx_t sc;
	time_t ret;

	hal_spinlockSet(&timer_common.lock, &sc);
	if (timer_common.genTmr.usable != 0U) {
		ret = _hal_genericTimerGetUs();
	}
	else {
		ret = hal_timerCyc2us(_hal_timerGetCyc());
	}
	hal_spinlockClear(&timer_common.lock, &sc);

	return ret;
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
	(void)hal_strncpy(features,
			(timer_common.genTmr.usable != 0U) ? "Using EPIT and GPT timers, Generic Timer as timestamp" :
												 "Using EPIT and GPT timers",
			len);
	features[len - 1U] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
	timer_common.epit1 = _pmap_halMapDevice(EPIT_BASE, 0, SIZE_PAGE);
	timer_common.gpt1 = _pmap_halMapDevice(GPT_BASE, 0, SIZE_PAGE);
	timer_common.timerhi = 0;

	hal_spinlockCreate(&timer_common.lock, "timer");

	timer_common.wakeuph.data = NULL;
	timer_common.wakeuph.n = TIMER_IRQ_ID;
	timer_common.wakeuph.f = timer_wakeupIrqHandler;

	(void)hal_interruptsSetHandler(&timer_common.wakeuph);

	timer_common.timerh.data = NULL;
	timer_common.timerh.n = GPT_IRQ;
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

	/* Last: it measures itself against the GPT, which has to be running */
	timer_common.genTmr.usable = 0;
	(void)_hal_genericTimerInit();
}
