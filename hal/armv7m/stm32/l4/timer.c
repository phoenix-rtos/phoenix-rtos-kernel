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
#define ARR_VAL   0xffff


enum { lptim_isr = 0, lptim_icr, lptim_ier, lptim_cfgr, lptim_cr, lptim_cmp, lptim_arr, lptim_cnt, lptim_or };


static struct {
	intr_handler_t overflowh;
	spinlock_t sp;

	volatile u32 *lptim;
	volatile time_t upper;

	intr_handler_t timerh;
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
	u32 isr = *(timer_common.lptim + lptim_isr), clr = 0;

	/* Clear CMPOK. Has to be done before active IRQs (errata) */
	if ((isr & (1 << 3)) != 0) {
		*(timer_common.lptim + lptim_icr) = (1 << 3);
		hal_cpuDataMemoryBarrier();
	}

	/* Clear ARRM */
	if ((isr & (1 << 1)) != 0) {
		++timer_common.upper;
		clr |= (1 << 1);
	}

	/* Clear CMPM */
	if ((isr & (1 << 0)) != 0) {
		clr |= (1 << 0);
	}

	*(timer_common.lptim + lptim_icr) = clr;

	hal_cpuDataMemoryBarrier();

	return 0;
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

	/* Check if we have unhandled overflow event */
	if (*(timer_common.lptim + lptim_isr) & (1 << 1)) {
		lower = timer_getCnt();
		if (lower != ARR_VAL) {
			++upper;
		}
	}

	hal_spinlockClear(&timer_common.sp, &sc);

	return (upper * (ARR_VAL + 1)) + lower;
}

/* Additional functions */

void timer_jiffiesAdd(time_t t)
{
	(void)t;
}


void timer_setAlarm(time_t us)
{
	u32 setval, timerval;
	spinlock_ctx_t sc;
	time_t ticks = hal_timerUs2Cyc(us);

	hal_spinlockSet(&timer_common.sp, &sc);

	timerval = timer_getCnt();

	/* Allow max 2/3 ARR_VAL sleep period, so no double ARR can occur */
	if (ticks > (ARR_VAL * 2) / 3) {
		ticks = (ARR_VAL * 2) / 3;
	}

	setval = (timerval + (u32)ticks) & ARR_VAL;

	/* Can't have cmp == arr */
	if (setval > ARR_VAL - 1) {
		setval = ARR_VAL - 1;
	}

	*(timer_common.lptim + lptim_cmp) = setval;
	hal_cpuDataMemoryBarrier();
	/* Wait for CMPOK */
	while ((*(timer_common.lptim + lptim_isr) & (1 << 3)) == 0) {
	}
	hal_cpuDataMemoryBarrier();

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
	int err;

	h->f = f;
	h->n = SYSTICK_IRQ;
	h->data = data;

	err = hal_interruptsSetHandler(h);

	/* Register LPTIM1 irq on system interrupt too to cause
	 * reschedule after wakeup ASAP */
	if (err == 0) {
		timer_common.timerh.f = f;
		timer_common.timerh.n = lptim1_irq;
		timer_common.timerh.data = data;
		timer_common.timerh.got = NULL;
		err = hal_interruptsSetHandler(&timer_common.timerh);
	}

	return err;
}


void _hal_timerInit(u32 interval)
{
	timer_common.lptim = (void *)0x40007c00;
	timer_common.upper = 0;

	hal_spinlockCreate(&timer_common.sp, "timer");

	*(timer_common.lptim + lptim_cr) = 0;
	hal_cpuDataMemoryBarrier();
	*(timer_common.lptim + lptim_cfgr) = (PRESCALER << 9);
	/* Enable CMPM and ARRM IRQs */
	*(timer_common.lptim + lptim_ier) = (1 << 1) | (1 << 0);
	hal_cpuDataMemoryBarrier();
	/* Timer enable */
	*(timer_common.lptim + lptim_cr) = 1;
	hal_cpuDataMemoryBarrier();
	*(timer_common.lptim + lptim_arr) = ARR_VAL;
	/* Wait for ARROK. Don't need to clear this ISR, we do it once */
	while ((*(timer_common.lptim + lptim_isr) & (1 << 4)) == 0) {
	}
	hal_cpuDataMemoryBarrier();

	timer_common.overflowh.f = timer_irqHandler;
	timer_common.overflowh.n = lptim1_irq;
	timer_common.overflowh.got = NULL;
	timer_common.overflowh.data = NULL;
	hal_interruptsSetHandler(&timer_common.overflowh);

	/* Trigger timer start */
	*(timer_common.lptim + lptim_cr) |= 4;
	hal_cpuDataMemoryBarrier();

	_stm32_systickInit(interval);
}
