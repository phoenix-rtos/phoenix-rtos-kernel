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
	lptim_ccmr1,
	lptim_ccr2 = 0xd,
};


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
	u32 isr = *(timer_common.lptim + lptim_isr), clr = 0;

	/* Clear CMPOK. Has to be done before active IRQs (errata) */
	if ((isr & (1U << 3)) != 0U) {
		*(timer_common.lptim + lptim_icr) = (1U << 3);
		hal_cpuDataMemoryBarrier();
	}

	/* Clear ARRM */
	if ((isr & (1U << 1)) != 0U) {
		++timer_common.upper;
		clr |= (1U << 1);
	}

	/* Clear CMPM */
	if ((isr & (1U << 0)) != 0U) {
		clr |= (1U << 0);
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
	if ((*(timer_common.lptim + lptim_isr) & (1U << 1)) != 0U) {
		lower = timer_getCnt();
		if (lower != RELOAD_VAL) {
			++upper;
		}
	}

	hal_spinlockClear(&timer_common.sp, &sc);

	return (upper * (time_t)(RELOAD_VAL + 1U)) + (time_t)lower;
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
	long cycle_len = hal_i2s(", cycle [us] ", cycle, (unsigned long)hal_timerCyc2us((time_t)RELOAD_VAL + 1), 10U, 0);

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
	/* Enable CMPM and ARRM IRQs */
	*(timer_common.lptim + lptim_ier) = (1U << 1) | (1U << 0);
	hal_cpuDataMemoryBarrier();
	/* Timer enable */
	*(timer_common.lptim + lptim_cr) = 1;
	hal_cpuDataMemoryBarrier();
	*(timer_common.lptim + lptim_arr) = RELOAD_VAL;
	/* Wait for ARROK. Don't need to clear this ISR, we do it once */
	while ((*(timer_common.lptim + lptim_isr) & (1U << 4)) == 0U) {
	}
	hal_cpuDataMemoryBarrier();

	timer_common.overflowh.f = timer_irqHandler;
	timer_common.overflowh.n = LPTIM_SYSTEM_IRQ;
	timer_common.overflowh.got = NULL;
	timer_common.overflowh.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.overflowh);

	/* Trigger timer start */
	*(timer_common.lptim + lptim_cr) |= 4U;
	hal_cpuDataMemoryBarrier();

	(void)_stm32_systickInit(interval);
}
