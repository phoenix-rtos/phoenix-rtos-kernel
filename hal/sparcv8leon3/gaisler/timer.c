/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Timer controller
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <hal/hal.h>
#include "hal/timer.h"
#include "hal/spinlock.h"
#include "hal/sparcv8leon3/sparcv8leon3.h"
#include "hal/string.h"

#ifdef NOMMU
#define VADDR_GPTIMER0 GPTIMER0_BASE
#else
#define VADDR_GPTIMER0 (void *)((u32)VADDR_PERIPH_BASE + PAGE_OFFS_GPTIMER0)
#endif

/* Timer control bitfields */

#define TIMER_ENABLE      (1 << 0)
#define TIMER_PERIODIC    (1 << 1)
#define TIMER_LOAD        (1 << 2)
#define TIMER_INT_ENABLE  (1 << 3)
#define TIMER_INT_PENDING (1 << 4)
#define TIMER_CHAIN       (1 << 5)


/* clang-format off */

enum { timer1 = 0, timer2, timer3, timer4 };

/* clang-format on */


struct {
	volatile u32 *timer0_base;
	intr_handler_t handler;
	volatile time_t jiffies;
	spinlock_t sp;
	u32 ticksPerFreq;
} timer_common;


static int _timer_irqHandler(unsigned int irq, cpu_context_t *ctx, void *data)
{
	volatile u32 st = *(timer_common.timer0_base + GPT_TCTRL1) & TIMER_INT_PENDING;

	if (st != 0) {
		++timer_common.jiffies;
		/* Clear irq status - set & clear to handle different GPTIMER core versions  */
		*(timer_common.timer0_base + GPT_TCTRL1) |= TIMER_INT_PENDING;
		hal_cpuDataStoreBarrier();
		*(timer_common.timer0_base + GPT_TCTRL1) &= ~TIMER_INT_PENDING;
		hal_cpuDataStoreBarrier();
	}

	return 0;
}


static inline void timer_setReloadValue(int timer, u32 val)
{
	*(timer_common.timer0_base + GPT_TRLDVAL1 + timer * 4) = val;
}


static void timer_setPrescaler(int timer, u32 freq)
{
	u32 prescaler = SYSCLK_FREQ / 1000000; /* 1 MHz */
	u32 ticks = (SYSCLK_FREQ / prescaler) / freq;

	timer_setReloadValue(timer, ticks - 1);
	*(timer_common.timer0_base + GPT_SRELOAD) = prescaler - 1;

	timer_common.ticksPerFreq = ticks;
}


time_t hal_timerGetUs(void)
{
	time_t val;
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.sp, &sc);
	val = timer_common.jiffies;
	hal_spinlockClear(&timer_common.sp, &sc);

	return val * 1000ULL;
}


void hal_timerSetWakeup(u32 waitUs)
{
}


int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = TIMER_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


char *hal_timerFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using default timer", len);
	return features;
}


void _hal_timerInit(u32 interval)
{
	timer_common.jiffies = 0;
	timer_common.timer0_base = VADDR_GPTIMER0;

	/* Disable timer interrupts - bits cleared when written 1 */
	volatile u32 st = *(timer_common.timer0_base + GPT_TCTRL1) & (TIMER_INT_ENABLE | TIMER_INT_PENDING);
	*(timer_common.timer0_base + GPT_TCTRL1) = st;
	/* Disable timer */
	*(timer_common.timer0_base + GPT_TCTRL1) = 0;
	/* Reset counter and reload value */
	*(timer_common.timer0_base + GPT_TCNTVAL1) = 0;
	*(timer_common.timer0_base + GPT_TRLDVAL1) = 0;

	timer_common.handler.f = NULL;
	timer_common.handler.n = TIMER_IRQ;
	timer_common.handler.data = NULL;

	timer_setPrescaler(timer1, interval);

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = TIMER_IRQ;
	timer_common.handler.data = NULL;
	hal_interruptsSetHandler(&timer_common.handler);

	/* Enable timer and interrupts */
	/* Load reload value into counter register */
	*(timer_common.timer0_base + GPT_TCTRL1) |= TIMER_ENABLE | TIMER_INT_ENABLE | TIMER_LOAD | TIMER_PERIODIC;
}
