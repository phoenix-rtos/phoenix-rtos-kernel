/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2021, 2023, 2026 Phoenix Systems
 * Author: Hubert Buczynski, Aleksander Kaminski, Wladyslaw Mlynik
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "hal/aarch64/aarch64.h"
#include "hal/aarch64/interrupts_gicv2.h"
#include "hal/cpu.h"
#include "hal/timer.h"
#include "hal/string.h"

#include "config.h"


static struct {
	intr_handler_t handler;
	intr_handler_t initHandler;
	u64 nextDeadline[NUM_CPUS];

	u64 freq;
	u32 ticksPerFreq;
} timer_common;


static int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	unsigned int cpuID = hal_cpuGetID();

	(void)n;
	(void)arg;
	(void)ctx;

	timer_common.nextDeadline[cpuID] += (u64)timer_common.ticksPerFreq * (u64)hal_cpuGetCount();
	sysreg_write(cntp_cval_el0, timer_common.nextDeadline[cpuID]);
	hal_cpuInstrBarrier();

	return 0;
}


static time_t hal_timerCyc2us(time_t cyc)
{
	time_t freq = (time_t)timer_common.freq;

	return ((cyc / freq) * 1000000LL) + (((cyc % freq) * 1000000LL) / freq);
}


static time_t hal_timerGetCyc(void)
{
	hal_cpuInstrBarrier();

	return (time_t)sysreg_read(cntpct_el0);
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
	(void)hal_strncpy(features, "Using ARM Generic Timer", len);
	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "`len` is always non-zero." */
	features[len - 1U] = '\0';
	return features;
}


static int _timer_initIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	unsigned int cpuID = hal_cpuGetID();
	u64 period = (u64)timer_common.ticksPerFreq * (u64)hal_cpuGetCount();
	u64 now = (u64)hal_timerGetCyc();
	u64 deadline = now - (now % period) + ((u64)cpuID * (u64)timer_common.ticksPerFreq);

	(void)n;
	(void)arg;
	(void)ctx;

	/* Each core gets its own slot within the period */
	if (deadline <= now) {
		deadline += period;
	}
	timer_common.nextDeadline[cpuID] = deadline;

	/* Set deadline and enable timer */
	sysreg_write(cntp_cval_el0, deadline);
	sysreg_write(cntp_ctl_el0, 1UL);
	hal_cpuInstrBarrier();

	/* PPI has to be enabled on each core separately */
	interrupts_enableIRQ(TIMER_IRQ_ID);

	return 0;
}


void _hal_timerInit(u32 interval)
{
	timer_common.freq = sysreg_read(cntfrq_el0);
	timer_common.ticksPerFreq = (u32)(timer_common.freq / ((u64)interval * (u64)hal_cpuGetCount()));

	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = TIMER_IRQ_ID;
	timer_common.handler.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.handler);

	timer_common.initHandler.f = _timer_initIrqHandler;
	timer_common.initHandler.n = TIMER_INIT_IRQ_ID;
	timer_common.initHandler.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.initHandler);

	/* Start timer on all cores, each core handles it once its interrupts are enabled */
	hal_cpuSendSGI(SGI_FLT_USE_LIST, (u8)((1U << hal_cpuGetCount()) - 1U), (u8)TIMER_INIT_IRQ_ID);
}
