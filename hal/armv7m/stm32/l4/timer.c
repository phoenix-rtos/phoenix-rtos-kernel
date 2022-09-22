/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2012, 2017, 2021 Phoenix Systems
 * Author: Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../config.h"
#include "../../armv7m.h"
#include "../../../timer.h"
#include "../../../interrupts.h"
#include "../../../spinlock.h"

/*
 * Prescaler settings (32768 Hz input frequency):
 * 0 - 1/1
 * 1 - 1/2
 * 2 - 1/4
 * 3 - 1/8
 * 4 - 1/16
 * 5 - 1/32
 * 6 - 1/64
 * 7 - 1/128
 */
#define PRESCALER 3


enum { lptim_isr = 0, lptim_icr, lptim_ier, lptim_cfgr, lptim_cr, lptim_cmp, lptim_arr, lptim_cnt, lptim_or };


static struct {
	intr_handler_t overflowh;
	spinlock_t sp;

	volatile u32 *lptim;
	volatile time_t upper;
	volatile int wakeup;
} timer_common;


static u32 timer_getCnt(void)
{
	u32 cnt[2];

	/* From documentation: "It should be noted that for a reliable LPTIM_CNT
	 * register read access, two consecutive read accesses must be performed and compared.
	 * A read access can be considered reliable when the
	 * values of the two consecutive read accesses are equal." */

	cnt[0] = *(timer_common.lptim + lptim_cnt);

	do {
		cnt[1] = cnt[0];
		cnt[0] = *(timer_common.lptim + lptim_cnt);
	} while (cnt[0] != cnt[1]);

	return cnt[0] & 0xffffu;
}


static int timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)ctx;
	(void)arg;
	int ret = 0;

	if (*(timer_common.lptim + lptim_isr) & (1 << 1)) {
		++timer_common.upper;
		*(timer_common.lptim + lptim_icr) = 2;
	}

	if (*(timer_common.lptim + lptim_isr) & 1) {
		*(timer_common.lptim + lptim_icr) = 1;

		if (timer_common.wakeup != 0) {
			ret = 1;
			timer_common.wakeup = 0;
		}
	}

	hal_cpuDataMemoryBarrier();

	return ret;
}


static time_t hal_timerCyc2Us(time_t ticks)
{
	return (ticks * 1000 * 1000) / (32768 / (1 << PRESCALER));
}


static time_t hal_timerUs2Cyc(time_t us)
{
	return ((32768 / (1 << PRESCALER)) * us + (500 * 1000)) / (1000 * 1000);
}


static time_t hal_timerGetCyc(void)
{
	time_t upper;
	u32 lower;
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.sp, &sc);
	upper = timer_common.upper;
	lower = timer_getCnt();

	if (*(timer_common.lptim + lptim_isr) & (1 << 1)) {
		/* Check if we have unhandled overflow event.
		 * If so, upper is one less than it should be */
		if (timer_getCnt() >= lower) {
			++upper;
		}
	}
	hal_spinlockClear(&timer_common.sp, &sc);

	return (upper << 16) + lower;
}

/* Additional functions */

void timer_jiffiesAdd(time_t t)
{
	(void)t;
}


void timer_setAlarm(time_t us)
{
	const u32 mintime = hal_timerUs2Cyc(1000);
	u32 setval, timerval[2];
	int arrpend;
	spinlock_ctx_t sc;
	time_t ticks = hal_timerUs2Cyc(us);

	if (ticks > 0xffff0000U) {
		ticks = 0xffff0000U;
	}

	hal_spinlockSet(&timer_common.sp, &sc);

	/* Check if there's pending overflow to be handled
	 * to not lose it should there be next when sleeping */
	timerval[0] = timer_getCnt();
	arrpend = (*(timer_common.lptim + lptim_isr) & 2) != 0 ? 1 : 0;
	timerval[1] = timer_getCnt();

	if (timerval[0] > timerval[1] || arrpend != 0) {
		++timer_common.upper;
		*(timer_common.lptim + lptim_icr) = 2;
	}

	if (ticks >= 0xffff) {
		setval = (timerval[1] - 1) & 0xffff;
	}
	else {
		setval = (timerval[1] + (u32)ticks) & 0xffff;

		if (((setval - timerval[1]) & 0xffff) < mintime) {
			/* There is too much risk that interrupt will arrive before wfi
			 * This can happen regardless of previous checks - can be
			 * caused by a modulo operation few lines above */
			setval = (timerval[1] + mintime) & 0xffff;
		}
	}

	/* Can't have cmp == arr */
	if (setval > 0xfffe) {
		setval = 0xfffe;
	}

	*(timer_common.lptim + lptim_cmp) = setval;
	hal_cpuDataMemoryBarrier();
	*(timer_common.lptim + lptim_icr) = 1;
	hal_cpuDataMemoryBarrier();
	timer_common.wakeup = 1;

	hal_spinlockClear(&timer_common.sp, &sc);
}


void hal_timerSetWakeup(u32 when)
{
}

/* Interface functions */

time_t hal_timerGetUs(void)
{
	return hal_timerCyc2Us(hal_timerGetCyc());
}


int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = SYSTICK_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


void _hal_timerInit(u32 interval)
{
	timer_common.lptim = (void *)0x40007c00;
	timer_common.upper = 0;
	timer_common.wakeup = 0;

	hal_spinlockCreate(&timer_common.sp, "timer");

	*(timer_common.lptim + lptim_cr) = 0;
	hal_cpuDataMemoryBarrier();
	*(timer_common.lptim + lptim_cfgr) = (PRESCALER << 9);
	*(timer_common.lptim + lptim_ier) = 3;
	*(timer_common.lptim + lptim_icr) |= 0x7f;
	hal_cpuDataMemoryBarrier();
	*(timer_common.lptim + lptim_cr) = 1;
	hal_cpuDataMemoryBarrier();
	*(timer_common.lptim + lptim_cnt) = 0;
	*(timer_common.lptim + lptim_cmp) = 0xfffe;
	*(timer_common.lptim + lptim_arr) = 0xffff;
	hal_cpuDataMemoryBarrier();

	timer_common.overflowh.f = timer_irqHandler;
	timer_common.overflowh.n = lptim1_irq;
	timer_common.overflowh.got = NULL;
	timer_common.overflowh.data = NULL;
	hal_interruptsSetHandler(&timer_common.overflowh);

	*(timer_common.lptim + lptim_cr) |= 4;
	hal_cpuDataMemoryBarrier();

	_stm32_systickInit(interval);
}
