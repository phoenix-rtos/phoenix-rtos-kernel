/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2021, 2023 Phoenix Systems
 * Author: Hubert Buczynski, Aleksander Kaminski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/aarch64/aarch64.h"
#include "hal/aarch64/interrupts_gicv2.h"
#include "hal/timer.h"
#include "hal/spinlock.h"
#include "hal/string.h"

#include <board_config.h>

#include "hal/cpu.h"

#include "config.h"

static struct {
	intr_handler_t handler;

	u32 ticksPerFreq;
	spinlock_t sp;
} timer_common;


static int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;

	spinlock_ctx_t sc;
	hal_spinlockSet(&timer_common.sp, &sc);

	hal_spinlockClear(&timer_common.sp, &sc);

	/* Clear the architectural timer interrupt condition by reloading countdown register */
	__asm__ volatile("msr cntp_tval_el0, %0" ::"r"((u64)timer_common.ticksPerFreq));

	u32 nextID = hal_cpuGetID() + 1U;
	u32 nextTargetCPU = (nextID == hal_cpuGetCount()) ? (u32)1U : ((u32)1U << nextID);
	interrupts_setCPU(n, nextTargetCPU);
	hal_cpuDataSyncBarrier();

	return 0;
}


static time_t hal_timerCyc2us(time_t cyc)
{
	return (cyc * 1000LL) / ((time_t)timer_common.ticksPerFreq * (time_t)hal_cpuGetCount());
}


static time_t hal_timerGetCyc(void)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.sp, &sc);

	u64 pct;
	__asm__ volatile("mrs %0, cntpct_el0" : "=r"(pct));

	hal_spinlockClear(&timer_common.sp, &sc);

	return pct;
}


void hal_timerSetWakeup(u32 waitUs)
{
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
	(void)hal_strncpy(features, "Using Architectural Virtual Timer", len);
	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "`len` is always non-zero." */
	features[len - 1U] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
	u64 freq;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
	if (freq == 0) {
		freq = 100000000;
	}

	timer_common.ticksPerFreq = freq / (interval * hal_cpuGetCount());

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = TIMER_IRQ_ID;
	timer_common.handler.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.handler);

	/* Set up initial architectural countdown value */
	__asm__ volatile("msr cntp_tval_el0, %0" ::"r"((u64)timer_common.ticksPerFreq));
	/* Enable the counter local to this core */
	__asm__ volatile("msr cntp_ctl_el0, %0" ::"r"(1));
}
