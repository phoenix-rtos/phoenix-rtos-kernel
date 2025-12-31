/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver using TTC peripheral
 *
 * Copyright 2021, 2023 Phoenix Systems
 * Author: Hubert Buczynski, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "timer_ttc_impl.h"
#include "hal/timer.h"
#include "hal/spinlock.h"
#include "hal/string.h"

#include "config.h"
#if defined(__CPU_ZYNQMP)
#include "zynqmp.h"
#elif defined(__CPU_ZYNQ7000)
#include "zynq.h"
#endif


static struct {
	volatile u32 *ttc;
	intr_handler_t handler;
	volatile u64 jiffies;

	u32 ticksPerFreq;
	spinlock_t sp;
} timer_common;


/* clang-format off */
enum {
	clk_ctrl = 0, clk_ctrl2, clk_ctrl3, cnt_ctrl, cnt_ctrl2, cnt_ctrl3, cnt_value, cnt_value2, cnt_value3, interval_val, interval_cnt2, interval_cnt3,
	match0, match1_cnt2, match1_cnt3, match1, match2_cnt2, match2_cnt3, match2, match3_cnt2, match3_cnt3, isr, irq_reg2, irq_reg3, ier, irq_en2,
	irq_en3, ev_ctrl_t1, ev_ctrl_t2, ev_ctrl_t3, ev_reg1, ev_reg2, ev_reg3
};
/* clang-format on */


static int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	u32 nextID;
	u32 nextTargetCPU;

	spinlock_ctx_t sc;
	hal_spinlockSet(&timer_common.sp, &sc);
	/* Interval IRQ */
	if ((*(timer_common.ttc + isr) & 1) != 0) {
		timer_common.jiffies += timer_common.ticksPerFreq;
	}

	hal_spinlockClear(&timer_common.sp, &sc);
#if NUM_CPUS != 1
	nextID = hal_cpuGetID() + 1;
	nextTargetCPU = (nextID == hal_cpuGetCount()) ? 1 : (1 << nextID);
	interrupts_setCPU(n, nextTargetCPU);
#else
	/* These variables are not necessary on single-core kernel */
	(void)nextID;
	(void)nextTargetCPU;
#endif
	hal_cpuDataSyncBarrier();

	return 0;
}


static time_t hal_timerCyc2us(u64 cyc)
{
	return (cyc * 1000ULL) / ((time_t)timer_common.ticksPerFreq * (time_t)hal_cpuGetCount());
}


static time_t hal_timerGetCyc(void)
{
	spinlock_ctx_t sc;
	u64 jiffies, cnt;

	hal_spinlockSet(&timer_common.sp, &sc);
	cnt = *(timer_common.ttc + cnt_value);
	jiffies = timer_common.jiffies;

	/* Check if there's pending jiffies increment */
	if ((*(timer_common.ttc + isr) & 0x1U) != 0U) {
		/* ISR register is clear on read, we have to update jiffies now */
		timer_common.jiffies += timer_common.ticksPerFreq;

		/* Timer might've just wrapped-around,
		 * take counter value again */
		jiffies = timer_common.jiffies;
		cnt = *(timer_common.ttc + cnt_value);
	}
	hal_spinlockClear(&timer_common.sp, &sc);

	return jiffies + cnt;
}


void hal_timerSetWakeup(u32 waitUs)
{
	/* Not implemented on this platform */
}


time_t hal_timerGetUs(void)
{
	return hal_timerCyc2us(hal_timerGetCyc());
}


int hal_timerRegister(intrFn_t f, void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = TIMER_IRQ_ID;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


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


char *hal_timerFeatures(char *features, unsigned int len)
{
	(void)hal_strncpy(features, "Using Triple Timer Counter", len);
	features[len - 1U] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
	timer_common.ttc = _zynq_ttc_getAddress();
	_zynq_ttc_performReset();

	timer_common.jiffies = 0;

	/* Disable timer */
	*(timer_common.ttc + clk_ctrl) = 0;

	/* Reset count control register */
	*(timer_common.ttc + cnt_ctrl) = 0x00000021U;

	/* Reset registers */
	*(timer_common.ttc + interval_val) = 0;
	*(timer_common.ttc + interval_cnt2) = 0;
	*(timer_common.ttc + interval_cnt3) = 0;
	*(timer_common.ttc + match0) = 0;
	*(timer_common.ttc + match1_cnt2) = 0;
	*(timer_common.ttc + match2_cnt3) = 0;
	*(timer_common.ttc + ier) = 0;
	*(timer_common.ttc + isr) = 0x1fU;

	/* Reset counters and restart counting */
	*(timer_common.ttc + cnt_ctrl) = 0x10U;

	hal_timerSetPrescaler(interval * hal_cpuGetCount());

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = TIMER_IRQ_ID;
	timer_common.handler.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.handler);

	*(timer_common.ttc + interval_val) |= timer_common.ticksPerFreq & 0xffffU;

	/* Reset counter */
	*(timer_common.ttc + cnt_ctrl) = 0x2U;
	/* Enable interval irq timer */
	*(timer_common.ttc + ier) = 0x1U;
}
