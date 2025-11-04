/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2024 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "config.h"
#include "hal/timer.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "include/arch/armv8m/mcx/n94x/mcxn94x.h"


void _interrupts_nvicSetPending(s8 irqn);


/* clang-format off */
enum { ostimer_evtimerl = 0, ostimer_evtimerh, ostimer_capturel, ostimer_captureh,
	ostimer_matchl, ostimer_matchh, ostimer_oseventctrl = 7 };
/* clang-format on */


static struct {
	volatile u32 *base;
	u32 high;
	u64 timerLast;
	u32 interval;
	spinlock_t lock;
} timer_common;


static u64 timer_gray2bin(u64 gray)
{
	return _mcxn94x_sysconGray2Bin(gray);
}


static u64 timer_bin2gray(u64 bin)
{
	return bin ^ (bin >> 1);
}


static time_t hal_timerCyc2Us(time_t ticks)
{
	return (ticks * 1000 * 1000) / 32768;
}


static time_t hal_timerUs2Cyc(time_t us)
{
	return (32768 * us + (500 * 1000)) / (1000 * 1000);
}


static u64 hal_timerGetCyc(void)
{
	u32 low = *(timer_common.base + ostimer_evtimerl);
	u32 high = *(timer_common.base + ostimer_evtimerh) & 0x3ff;
	u64 timerval;

	timerval = timer_gray2bin((u64)low | ((u64)high << 32));
	if (timerval < timer_common.timerLast) {
		/* Once every ~4 years */
		timer_common.high += 1 << (42 - 32);
	}
	timer_common.timerLast = timerval;

	return timerval | timer_common.high;
}


/* Interface functions */


void hal_timerSetWakeup(u32 waitUs)
{
	u64 val, valgray, inc;
	spinlock_ctx_t sc;

	if (waitUs >= timer_common.interval) {
		waitUs = timer_common.interval;
	}

	hal_spinlockSet(&timer_common.lock, &sc);

	/* Clear IRQ flag */
	*(timer_common.base + ostimer_oseventctrl) |= 1;
	hal_cpuDataMemoryBarrier();

	/* Wait for MATCH to be write ready (should be instant) */
	while ((*(timer_common.base + ostimer_oseventctrl) & (1 << 2)) != 0) {
	}

	inc = hal_timerUs2Cyc(waitUs);
	val = hal_timerGetCyc() + inc;
	valgray = timer_bin2gray(val);

	/* Write new MATCH value */
	*(timer_common.base + ostimer_matchl) = valgray & 0xffffffffuL;
	*(timer_common.base + ostimer_matchh) = (valgray >> 32) & 0x3ff;
	hal_cpuDataMemoryBarrier();

	/* Wait for MATCH value transfer from shadow */
	while ((*(timer_common.base + ostimer_oseventctrl) & (1 << 2)) != 0) {
	}

	if ((hal_timerGetCyc() >= val) && (((val >> 32) & 0x400) == 0) &&
			((*(timer_common.base + ostimer_oseventctrl) & 1) == 0)) {
		/* We just missed the timer value and be the interrupt won't
		 * be generated. Trigger the interrupt manually instead. */
		_hal_scsIRQPendingSet(ostimer0_irq - 0x10);
	}

	hal_spinlockClear(&timer_common.lock, &sc);
}


time_t hal_timerGetUs(void)
{
	time_t ret;
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.lock, &sc);
	ret = hal_timerCyc2Us(hal_timerGetCyc());
	hal_spinlockClear(&timer_common.lock, &sc);

	return ret;
}


int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = ostimer0_irq;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


char *hal_timerFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using OSTIMER", len);
	features[len - 1] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
	timer_common.base = (void *)0x40049000;
	timer_common.timerLast = 0;
	timer_common.high = 0;
	timer_common.interval = interval;

	/* Use xtal32k clock source, enable the clock, deassert reset */
	_mcxn94x_sysconSetDevClk(pctl_ostimer, 1, 0, 1);
	_mcxn94x_sysconDevReset(pctl_ostimer, 0);

	/* Enable MATCH interrupt */
	*(timer_common.base + ostimer_oseventctrl) |= (1 << 1) | 1;

	hal_spinlockCreate(&timer_common.lock, "timer");
}
