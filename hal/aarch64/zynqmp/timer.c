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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/aarch64/aarch64.h"
#include "hal/aarch64/interrupts_gicv2.h"
#include "hal/timer.h"
#include "hal/spinlock.h"
#include "hal/string.h"

#include <board_config.h>

#ifdef ZYNQMP_VIRT

#include "hal/cpu.h"

#else

#include "hal/aarch64/arch/pmap.h"
#include "zynqmp.h"

#endif

#include "config.h"

#ifndef ZYNQMP_VIRT

#define TTC0_BASE_ADDR       ((addr_t)0x00ff110000U)
#define TIMER_SRC_CLK_CPU_1x 99990000U

#endif

static struct {
#ifndef ZYNQMP_VIRT
	volatile u32 *ttc;
#endif
	intr_handler_t handler;
	volatile time_t jiffies;

	u32 ticksPerFreq;
	spinlock_t sp;
} timer_common;


#ifndef ZYNQMP_VIRT
/* clang-format off */
enum {
	clk_ctrl = 0, clk_ctrl2, clk_ctrl3, cnt_ctrl, cnt_ctrl2, cnt_ctrl3, cnt_value, cnt_value2, cnt_value3, interval_val, interval_cnt2, interval_cnt3,
	match0, match1_cnt2, match1_cnt3, match1, match2_cnt2, match2_cnt3, match2, match3_cnt2, match3_cnt3, isr, irq_reg2, irq_reg3, ier, irq_en2,
	irq_en3, ev_ctrl_t1, ev_ctrl_t2, ev_ctrl_t3, ev_reg1, ev_reg2, ev_reg3
};
/* clang-format on */

#endif

static int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	spinlock_ctx_t sc;
	hal_spinlockSet(&timer_common.sp, &sc);

#ifdef ZYNQMP_VIRT
	u64 ctl;
	/* Read virtual timer control register to check the pending ISTATUS (bit 2) */
	__asm__ volatile("mrs %0, cntv_ctl_el0" : "=r"(ctl));
	timer_common.jiffies += timer_common.ticksPerFreq;
#else
	/* Interval IRQ */
	if ((*(timer_common.ttc + isr) & 1U) != 0U) {
		timer_common.jiffies += (time_t)timer_common.ticksPerFreq;
	}
#endif

	hal_spinlockClear(&timer_common.sp, &sc);

#ifdef ZYNQMP_VIRT
	/* Clear the architectural timer interrupt condition by reloading countdown register */
	__asm__ volatile("msr cntv_tval_el0, %0" :: "r"((u64)timer_common.ticksPerFreq));
#endif

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
	time_t jiffies, cnt;

	hal_spinlockSet(&timer_common.sp, &sc);

#ifdef ZYNQMP_VIRT
	u64 vct;
	/* Read absolute counter and compute relative delta to mimic TTC0's cnt_value tracking */
	__asm__ volatile("mrs %0, cntvct_el0" : "=r"(vct));
	cnt = (time_t)(vct - timer_common.jiffies);
	jiffies = timer_common.jiffies;

#else
	cnt = (time_t)(*(timer_common.ttc + cnt_value));
	jiffies = timer_common.jiffies;

	/* Check if there's pending jiffies increment */
	if ((*(timer_common.ttc + isr) & 1U) != 0U) {
		/* ISR register is clear on read, we have to update jiffies now */
		timer_common.jiffies += (time_t)timer_common.ticksPerFreq;

		/* Timer might've just wrapped-around,
		 * take counter value again */
		jiffies = timer_common.jiffies;
		cnt = (time_t)(*(timer_common.ttc + cnt_value));
	}
#endif

	hal_spinlockClear(&timer_common.sp, &sc);

	return jiffies + cnt;
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


#ifndef ZYNQMP_VIRT
static void hal_timerSetPrescaler(u32 freq)
{
	u32 ticks = TIMER_SRC_CLK_CPU_1x / freq;
	u32 prescaler = 0;

	while ((ticks >= 0xffffU) && (prescaler < 0x10U)) {
		prescaler++;
		ticks /= 2U;
	}

	if (prescaler != 0U) {
		/* Enable and set prescaler */
		prescaler--;
		*(timer_common.ttc + clk_ctrl) = (*(timer_common.ttc + clk_ctrl) & ~0x1fU) | (prescaler << 1);
		*(timer_common.ttc + clk_ctrl) |= 0x1U;
	}

	timer_common.ticksPerFreq = ticks;
}
#endif


char *hal_timerFeatures(char *features, size_t len)
{
#ifdef ZYNQMP_QEMU
	(void)hal_strncpy(features, "Using Architectural Virtual Timer Analogue", len);
#else
	(void)hal_strncpy(features, "Using Triple Timer Counter", len);
#endif
	/* parasoft-suppress-next-line MISRAC2012-DIR_4_1 "`len` is always non-zero." */
	features[len - 1U] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
#ifndef ZYNQMP_VIRT
	timer_common.ttc = _pmap_halMapDevice(TTC0_BASE_ADDR, 0, SIZE_PAGE);
	(void)_zynq_setDevRst(pctl_devreset_lpd_ttc0, 0);
#endif
	timer_common.jiffies = 0;

#ifdef ZYNQMP_VIRT
	u64 freq;
	__asm__ volatile("mrs %0, cntfrq_el0" : "=r"(freq));
	if (freq == 0) {
		freq = 100000000;
	}

	timer_common.ticksPerFreq = freq / (interval * hal_cpuGetCount());
#else
	/* Disable timer */
	*(timer_common.ttc + clk_ctrl) = 0;

	/* Reset count control register */
	*(timer_common.ttc + cnt_ctrl) = 0x00000021;

	/* Reset registers */
	*(timer_common.ttc + interval_val) = 0;
	*(timer_common.ttc + interval_cnt2) = 0;
	*(timer_common.ttc + interval_cnt3) = 0;
	*(timer_common.ttc + match0) = 0;
	*(timer_common.ttc + match1_cnt2) = 0;
	*(timer_common.ttc + match2_cnt3) = 0;
	*(timer_common.ttc + ier) = 0;
	*(timer_common.ttc + isr) = 0x1f;

	/* Reset counters and restart counting */
	*(timer_common.ttc + cnt_ctrl) = 0x10;

	hal_timerSetPrescaler(interval * hal_cpuGetCount());
#endif

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = TIMER_IRQ_ID;
	timer_common.handler.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.handler);

#ifdef ZYNQMP_VIRT
	/* Set up initial architectural countdown value */
	__asm__ volatile("msr cntv_tval_el0, %0" :: "r"((u64)timer_common.ticksPerFreq));
	/* Enable the counter local to this core */
	__asm__ volatile("msr cntv_ctl_el0, %0" :: "r"(1));
#else
	*(timer_common.ttc + interval_val) |= timer_common.ticksPerFreq & 0xffffU;

	/* Reset counter */
	*(timer_common.ttc + cnt_ctrl) = 0x2;
	/* Enable interval irq timer */
	*(timer_common.ttc + ier) = 0x1;
#endif
}
