/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../../cpu.h"
#include "../../spinlock.h"
#include "../../interrupts.h"

#define TTC_SRC_CLK_CPU_1x 111111115 /* Hz */

struct {
	volatile u32 *ttc;
	intr_handler_t handler;
	volatile time_t jiffies;

	u32 ticksPerFreq;
	spinlock_t sp;
} timer_common;


enum {
	clk_ctrl = 0, clk_ctrl2, clk_ctrl3, cnt_ctrl, cnt_ctrl2, cnt_ctrl3, cnt_value, cnt_value2, cnt_value3, interval_val, interval_cnt2, interval_cnt3,
	match0, match1_cnt2, match1_cnt3, match1, match2_cnt2, match2_cnt3, match2, match3_cnt2, match3_cnt3, isr, irq_reg2, irq_reg3, ier, irq_en2,
	irq_en3, ev_ctrl_t1, ev_ctrl_t2, ev_ctrl_t3, ev_reg1, ev_reg2, ev_reg3
};


extern unsigned int _end;


void timer_jiffiesAdd(time_t t)
{
	(void)t;
}


void timer_setAlarm(time_t us)
{
	(void)us;
}


static int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	u32 st = *(timer_common.ttc + isr);

	/* Interval IRQ */
	if (st & 0x1)
		timer_common.jiffies += timer_common.ticksPerFreq;

	/* Clear irq status */
	*(timer_common.ttc + isr) = st;

	hal_cpuDataSyncBarrier();

	return 0;
}


void hal_setWakeup(u32 when)
{

}


time_t hal_getTimer(void)
{
	spinlock_ctx_t sc;
	time_t ret;

	hal_spinlockSet(&timer_common.sp, &sc);
	ret = timer_common.jiffies;
	hal_spinlockClear(&timer_common.sp, &sc);

	return ret;
}


static void timer_setPrescaler(u32 freq)
{
	u32 ticks = TTC_SRC_CLK_CPU_1x / freq;
	u32 prescaler = 0;

	while ((ticks >= 0xffff) && (prescaler < 0x10)) {
		prescaler++;
		ticks /= 2;
	}

	if (prescaler) {
		/* Enable and set prescaler */
		prescaler--;
		*(timer_common.ttc + clk_ctrl) = (*(timer_common.ttc + clk_ctrl) & ~0x1f) | (prescaler << 1);
		*(timer_common.ttc + clk_ctrl) |= 0x1;
	}

	timer_common.ticksPerFreq = ticks;
}


void _timer_init(u32 interval)
{
	timer_common.ttc = (void *)(((u32)&_end + 10 * SIZE_PAGE - 1) & ~(SIZE_PAGE - 1));
	timer_common.jiffies = 0;

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

	timer_setPrescaler(interval);

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = 42;
	timer_common.handler.data = NULL;
	hal_interruptsSetHandler(&timer_common.handler);

	*(timer_common.ttc + interval_val) |= timer_common.ticksPerFreq & 0xffff;

	/* Reset counter */
	*(timer_common.ttc + cnt_ctrl) = 0x2;
	/* Enable interval irq timer */
	*(timer_common.ttc + ier) = 0x1;
}
