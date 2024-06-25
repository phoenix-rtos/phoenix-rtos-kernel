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
#include "hal/armv8m/armv8m.h"
#include "hal/timer.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "include/arch/armv8m/mcx/n94x/mcxn94x.h"


enum { ostimer_evtimerl = 0, ostimer_evtimerh, ostimer_capturel, ostimer_captureh,
	ostimer_matchl, ostimer_matchh, ostimer_oseventctrl };


static struct {
	volatile u32 *base;
	u32 high;
	u64 timerLast;
	u32 interval;
	spinlock_t lock;
} timer_common;


static u64 timer_gray2bin(u64 gray)
{
	u64 bin = 0;

	while (gray != 0) {
		bin ^= gray;
		gray >>= 1;
	}

	return bin;
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
	u32 high, low;
	u64 timerval;

	high = *(timer_common.base + ostimer_evtimerh);
	low = *(timer_common.base + ostimer_evtimerl);

	if (high != *(timer_common.base + ostimer_evtimerh)) {
		/* Rollover, read again */
		high = *(timer_common.base + ostimer_evtimerh);
		low = *(timer_common.base + ostimer_evtimerl);
	}

	timerval = timer_gray2bin(low | ((u64)high << 32));
	if (timerval < timer_common.timerLast) {
		/* Once every ~4 years */
		timer_common.high += 1 << (42 - 32);
	}

	timerval |= timer_common.high;

	return timerval;
}


/* Interface functions */


void hal_timerSetWakeup(u32 waitUs)
{
	u64 val, inc;
	spinlock_ctx_t sc;

	if (waitUs >= timer_common.interval) {
		waitUs = timer_common.interval;
	}

	hal_spinlockSet(&timer_common.lock, &sc);
	inc = hal_timerUs2Cyc(waitUs);
	val = timer_bin2gray(hal_timerGetCyc() + inc);

	/* Clear IRQ flag */
	*(timer_common.base + ostimer_oseventctrl) |= 1;
	hal_cpuDataMemoryBarrier();

	/* Write new MATCH value */
	*(timer_common.base + ostimer_matchl) = val & 0xffffffffuL;
	hal_cpuDataMemoryBarrier();
	*(timer_common.base + ostimer_matchh) = (val >> 32) & 0x3ff;
	hal_cpuDataMemoryBarrier();

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
	hal_spinlockCreate(&timer_common.lock, "timer");
}
