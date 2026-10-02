/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver for LPTIM
 *
 * Copyright 2012, 2017, 2021 Phoenix Systems
 * Copyright 2026 Apator Metrix
 * Author: Jakub Sejdak, Aleksander Kaminski, Mateusz Karcz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "hal/armv8m/stm32/config.h"
#include "hal/timer.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/string.h"


#define PRESC_SHIFT 3UL
#define RELOAD_VAL  (((LPTIM_SYSTEM_INPUT >> PRESC_SHIFT) * LPTIM_SYSTEM_CYCLE_MS / 1000UL) - 1UL)
#define TICKS_PER_CYCLE ((time_t)(RELOAD_VAL + 1UL))

#if RELOAD_VAL > 0xffffUL
#error Impossible cycle duration!
#endif


enum lptim_regs {
	lptim_isr = 0x0,
	lptim_icr,
	lptim_ier,
	lptim_cfgr,
	lptim_cr,
	lptim_ccr1,
	lptim_arr,
	lptim_cnt,
	lptim_cfgr2 = 0x9,
	lptim_rcr,
};

/* Interrupt status/enable/clear bits */
#define LPTIM_CC1IF   (1UL << 0)
#define LPTIM_ARRM    (1UL << 1)
#define LPTIM_EXTTRIG (1UL << 2)
#define LPTIM_CMP1OK  (1UL << 3)
#define LPTIM_ARROK   (1UL << 4)
#define LPTIM_UP      (1UL << 5)
#define LPTIM_DOWN    (1UL << 6)
#define LPTIM_UE      (1UL << 7)
#define LPTIM_REPOK   (1UL << 8)
#define LPTIM_IEROK   (1UL << 24) /* Note: this bit is not valid for lptim_ier register */


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

	/* From RM0487, 42.7.14: "When the LPTIM is running, reading the LPTIM_CNT
	 * register may return unreliable values. In this case it is necessary to
	 * perform consecutive reads until two returned values are identical." */

	cnt[0] = *(timer_common.lptim + lptim_cnt);

	do {
		cnt[1] = cnt[0];
		cnt[0] = *(timer_common.lptim + lptim_cnt);
	} while (cnt[0] != cnt[1]);

	return cnt[0] & 0xffffU;
}


static int timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)ctx;
	(void)arg;
	u32 isr, clr;

	isr = *(timer_common.lptim + lptim_isr);
	clr = 0;

	/* Timer overflow */
	if ((isr & LPTIM_ARRM) != 0U) {
		timer_common.upper += TICKS_PER_CYCLE;
		clr |= LPTIM_ARRM;
	}

	if ((isr & LPTIM_CC1IF) != 0U) {
		clr |= LPTIM_CC1IF;
	}

	*(timer_common.lptim + lptim_icr) = clr;

	hal_cpuDataMemoryBarrier();

	return 0;
}


static time_t hal_timerCyc2us(time_t ticks)
{
	return (ticks * 1000 * 1000) / (LPTIM_SYSTEM_INPUT / (time_t)(1U << PRESC_SHIFT));
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
	if ((*(timer_common.lptim + lptim_isr) & LPTIM_ARRM) != 0U) {
		lower = timer_getCnt();
		if (lower != RELOAD_VAL) {
			upper += TICKS_PER_CYCLE;
		}
	}

	hal_spinlockClear(&timer_common.sp, &sc);

	return upper + (time_t)lower;
}

/* Additional functions */

void hal_timerSetWakeup(u32 waitUs)
{
}

/* Interface functions */

time_t hal_timerGetUs(void)
{
	return hal_timerCyc2us(hal_timerGetCyc());
}


int hal_timerRegister(intrFn_t f, void *data, intr_handler_t *h)
{
	int err;

	h->f = f;
	h->n = SYSTICK_IRQ;
	h->data = data;
	err = hal_interruptsSetHandler(h);

	if (err == 0) {
		/* Register LPTIM irq on system interrupt too to cause
		 * reschedule after wakeup ASAP */
		timer_common.timerh.f = f;
		timer_common.timerh.n = LPTIM_SYSTEM_IRQ;
		timer_common.timerh.data = data;
		timer_common.timerh.got = NULL;
		err = hal_interruptsSetHandler(&timer_common.timerh);
		if (err != 0) {
			(void)hal_interruptsDeleteHandler(h);
		}
	}

	return err;
}


char *hal_timerFeatures(char *features, size_t len)
{
	char cycle[40];
	long cycle_len = hal_i2s(", cycle [us] ", cycle, (unsigned long)hal_timerCyc2us(TICKS_PER_CYCLE), 10U, 0);

	(void)hal_strncpy(features, "Using Low-Power Timer", len);
	if (len > (21 + cycle_len)) {
		cycle[cycle_len] = '\0';
		(void)hal_strncpy(features + 21, cycle, len - 21);
	}

	features[len - 1U] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
	timer_common.lptim = LPTIM_SYSTEM_BASE;
	timer_common.upper = 0;

	(void)_stm32_rccSetIPClk(LPTIM_SYSTEM_IPCLK_SEL, LPTIM_SYSTEM_IPCLK_VAL);
	(void)_stm32_rccSetDevClock(LPTIM_SYSTEM_PCTL, 1, 1);
	(void)_stm32_dbgmcuStopTimerInDebug(LPTIM_SYSTEM_PCTL, 1U);

	hal_spinlockCreate(&timer_common.sp, "timer");

	*(timer_common.lptim + lptim_cr) = 0;
	hal_cpuDataMemoryBarrier();
	*(timer_common.lptim + lptim_cfgr) = (PRESC_SHIFT << 9);
	*(timer_common.lptim + lptim_ier) = LPTIM_CC1IF | LPTIM_ARRM;
	hal_cpuDataMemoryBarrier();
	/* Timer enable */
	*(timer_common.lptim + lptim_cr) = 1;
	hal_cpuDataMemoryBarrier();
	*(timer_common.lptim + lptim_arr) = RELOAD_VAL;
	while ((*(timer_common.lptim + lptim_isr) & LPTIM_ARROK) == 0U) {
		/* Wait for ARROK status bit */
	}

	*(timer_common.lptim + lptim_icr) = LPTIM_ARROK; /* Not strictly necessary because we don't update ARR again */
	hal_cpuDataMemoryBarrier();

	timer_common.overflowh.f = timer_irqHandler;
	timer_common.overflowh.n = LPTIM_SYSTEM_IRQ;
	timer_common.overflowh.got = NULL;
	timer_common.overflowh.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.overflowh);

	/* Start timer in continuous mode */
	*(timer_common.lptim + lptim_cr) |= (1UL << 2);
	hal_cpuDataMemoryBarrier();

	(void)_stm32_systickInit(interval);
}


static time_t hal_timerUs2Cyc(time_t us)
{
	return (((time_t)LPTIM_SYSTEM_INPUT / (1LL << PRESC_SHIFT)) * us + (500 * 1000)) / (1000 * 1000);
}


void timer_setAlarm(time_t us)
{
	u32 setval, timerval, oldval;
	spinlock_ctx_t sc;
	time_t ticks = hal_timerUs2Cyc(us);

	hal_spinlockSet(&timer_common.sp, &sc);

	timerval = timer_getCnt();

	oldval = *(timer_common.lptim + lptim_ccr1);
	setval = timerval + (u32)ticks; /* We will check ticks value for potential overflow later */
	/*
	 * Value of CCR1 must be < ARR. If we are asked to sleep longer than that we will be woken up by ARRM anyway.
	 * In those cases we set the CCR1 register to either (CNT - 1) or 0 to save ourselves one needless wakeup.
	 */
	if ((ticks >= (time_t)RELOAD_VAL) || (setval >= RELOAD_VAL)) {
		setval = (timerval > 1U) ? (timerval - 1U) : 0U;
	}

	/*
	 * Only change CCR1 if the value is actually different - this is an optimization,
	 * but also trying to write CCR1 to the same value may never trigger CMP1OK.
	 */
	if (setval != oldval) {
		*(timer_common.lptim + lptim_ccr1) = setval;
		hal_cpuDataSyncBarrier();

		while ((*(timer_common.lptim + lptim_isr) & LPTIM_CMP1OK) == 0U) {
			/* Wait for CMP1OK */
		}

		*(timer_common.lptim + lptim_icr) = LPTIM_CMP1OK;
	}

	hal_spinlockClear(&timer_common.sp, &sc);
}
