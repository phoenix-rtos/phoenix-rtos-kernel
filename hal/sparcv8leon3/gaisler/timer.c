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


/* Timer control bitfields */

#define TIMER_ENABLE      (1 << 0)
#define TIMER_PERIODIC    (1 << 1)
#define TIMER_LOAD        (1 << 2)
#define TIMER_INT_ENABLE  (1 << 3)
#define TIMER_INT_PENDING (1 << 4)
#define TIMER_CHAIN       (1 << 5)

/* Timer registers */

#define GPT_SCALER     0             /* Scaler value register                 : 0x00 */
#define GPT_SRELOAD    1             /* Scaler reload value register          : 0x04 */
#define GPT_CONFIG     2             /* Configuration register                : 0x08 */
#define GPT_LATCHCFG   3             /* Latch configuration register          : 0x0C */
#define GPT_TCNTVAL(n) (n * 4)       /* Timer n counter value reg (n=1,2,...) : 0xn0 */
#define GPT_TRLDVAL(n) ((n * 4) + 1) /* Timer n reload value register         : 0xn4 */
#define GPT_TCTRL(n)   ((n * 4) + 2) /* Timer n control register              : 0xn8 */
#define GPT_TLATCH(n)  ((n * 4) + 3) /* Timer n latch register                : 0xnC */

#define TIMER_DEFAULT 1


static struct {
	volatile u32 *timer0_base;
	intr_handler_t handler;
	volatile time_t jiffies;
	spinlock_t sp;
	u32 ticksPerFreq;
} timer_common;


static int _timer_irqHandler(unsigned int irq, cpu_context_t *ctx, void *data)
{
	volatile u32 st = *(timer_common.timer0_base + GPT_TCTRL(TIMER_DEFAULT)) & TIMER_INT_PENDING;

	if (st != 0) {
		++timer_common.jiffies;
		/* Clear irq status - set & clear to handle different GPTIMER core versions  */
		*(timer_common.timer0_base + GPT_TCTRL(TIMER_DEFAULT)) |= TIMER_INT_PENDING;
		hal_cpuDataStoreBarrier();
		*(timer_common.timer0_base + GPT_TCTRL(TIMER_DEFAULT)) &= ~TIMER_INT_PENDING;
		hal_cpuDataStoreBarrier();
	}

	return 0;
}


static inline void timer_setReloadValue(int timer, u32 val)
{
	*(timer_common.timer0_base + GPT_TRLDVAL(timer)) = val;
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
	hal_strncpy(features, "Using General Purpose Timer", len);
	features[len - 1] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
	volatile u32 st;

	timer_common.jiffies = 0;

	timer_common.timer0_base = _pmap_halMapDevice(PAGE_ALIGN(GPTIMER0_BASE), PAGE_OFFS(GPTIMER0_BASE), SIZE_PAGE);

	/* Disable timer interrupts - bits cleared when written 1 */
	st = *(timer_common.timer0_base + GPT_TCTRL(TIMER_DEFAULT)) & (TIMER_INT_ENABLE | TIMER_INT_PENDING);
	*(timer_common.timer0_base + GPT_TCTRL(TIMER_DEFAULT)) = st;
	/* Disable timer */
	*(timer_common.timer0_base + GPT_TCTRL(TIMER_DEFAULT)) = 0;
	/* Reset counter and reload value */
	*(timer_common.timer0_base + GPT_TCNTVAL(TIMER_DEFAULT)) = 0;
	timer_setReloadValue(TIMER_DEFAULT, 0);

	timer_common.handler.f = NULL;
	timer_common.handler.n = TIMER_IRQ;
	timer_common.handler.data = NULL;

	timer_setPrescaler(TIMER_DEFAULT, interval);

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = TIMER_IRQ;
	timer_common.handler.data = NULL;
	hal_interruptsSetHandler(&timer_common.handler);

	/* Enable timer and interrupts */
	/* Load reload value into counter register */
	*(timer_common.timer0_base + GPT_TCTRL(TIMER_DEFAULT)) |= TIMER_ENABLE | TIMER_INT_ENABLE | TIMER_LOAD | TIMER_PERIODIC;
}
