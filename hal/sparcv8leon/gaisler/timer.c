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
#include "hal/sparcv8leon/sparcv8leon.h"
#include "hal/string.h"


/* Timer control bitfields */

#define TIMER_ENABLE      (1U << 0)
#define TIMER_PERIODIC    (1U << 1)
#define TIMER_LOAD        (1U << 2)
#define TIMER_INT_ENABLE  (1U << 3)
#define TIMER_INT_PENDING (1U << 4)
#define TIMER_CHAIN       (1U << 5)

/* Timer registers */

#define GPT_SCALER     0               /* Scaler value register                 : 0x00 */
#define GPT_SRELOAD    1               /* Scaler reload value register          : 0x04 */
#define GPT_CONFIG     2               /* Configuration register                : 0x08 */
#define GPT_LATCHCFG   3               /* Latch configuration register          : 0x0C */
#define GPT_TCNTVAL(n) ((n) * 4)       /* Timer n counter value reg (n=1,2,...) : 0xn0 */
#define GPT_TRLDVAL(n) (((n) * 4) + 1) /* Timer n reload value register         : 0xn4 */
#define GPT_TCTRL(n)   (((n) * 4) + 2) /* Timer n control register              : 0xn8 */
#define GPT_TLATCH(n)  (((n) * 4) + 3) /* Timer n latch register                : 0xnC */

#define TIMER_TIMEBASE 1
#define TIMER_WAKEUP   2

#define TIMEBASE_INTERVAL 0xffffffffU


static struct {
	volatile u32 *timer0_base;
	u32 wdog;
	intr_handler_t timebaseHandler;
	intr_handler_t wakeupHandler;
	volatile time_t jiffies;
	spinlock_t sp;
	u64 ticksPerFreq;
} timer_common;


static void timer_clearIrq(int timer)
{
	/* Clear irq status - set & clear to handle different GPTIMER core versions  */
	*(timer_common.timer0_base + GPT_TCTRL(timer)) |= TIMER_INT_PENDING;
	hal_cpuDataStoreBarrier();
	*(timer_common.timer0_base + GPT_TCTRL(timer)) &= ~TIMER_INT_PENDING;
	hal_cpuDataStoreBarrier();
}


static int _timer_irqHandler(unsigned int irq, cpu_context_t *ctx, void *data)
{
	u32 st;
	spinlock_ctx_t sc;
	int timer = (irq == (unsigned int)TIMER0_1_IRQ) ? TIMER_TIMEBASE : TIMER_WAKEUP;
	int ret = 0;

	hal_spinlockSet(&timer_common.sp, &sc);

	st = *(timer_common.timer0_base + GPT_TCTRL(timer)) & TIMER_INT_PENDING;

	if (st != 0U) {
		if (timer == TIMER_TIMEBASE) {
			++timer_common.jiffies;
		}
		else {
			ret = 1;
		}
		timer_clearIrq(timer);

#ifdef __CPU_GR740
		/* Reload watchdog (on GR740 there's a fixed PLL watchdog, restarted on watchdog timer tctrl write) */
		*(timer_common.timer0_base + GPT_TCTRL(timer_common.wdog)) |= TIMER_LOAD;
#endif
	}

	hal_spinlockClear(&timer_common.sp, &sc);

	return ret;
}


static inline void timer_setReloadValue(int timer, u32 val)
{
	*(timer_common.timer0_base + GPT_TRLDVAL(timer)) = val;
}


time_t hal_timerGetUs(void)
{
	u32 regVal;
	time_t val;
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.sp, &sc);

	regVal = *(timer_common.timer0_base + GPT_TCNTVAL(TIMER_TIMEBASE));

	/* Check if there's pending irq */
	if ((*(timer_common.timer0_base + GPT_TCTRL(TIMER_TIMEBASE)) & TIMER_INT_PENDING) != 0U) {
		++timer_common.jiffies;
		timer_clearIrq(TIMER_TIMEBASE);
		/* Timer might've just wrapped-around, take counter value again */
		regVal = *(timer_common.timer0_base + GPT_TCNTVAL(TIMER_TIMEBASE));
	}
	val = timer_common.jiffies;

	hal_spinlockClear(&timer_common.sp, &sc);

	return val * (time_t)(u64)(timer_common.ticksPerFreq + timer_common.ticksPerFreq - regVal);
}


void hal_timerSetWakeup(u32 waitUs)
{
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.sp, &sc);

	/* Disable timer */
	*(timer_common.timer0_base + GPT_TCTRL(TIMER_WAKEUP)) = 0;
	timer_clearIrq(TIMER_WAKEUP);

	/* Configure one shot timer */
	timer_setReloadValue(TIMER_WAKEUP, waitUs - 1U);
	*(timer_common.timer0_base + GPT_TCTRL(TIMER_WAKEUP)) = TIMER_ENABLE | TIMER_INT_ENABLE | TIMER_LOAD;

	hal_spinlockClear(&timer_common.sp, &sc);
}


void hal_timerWdogReboot(void)
{
	/* Reboot system using watchdog */
	*(timer_common.timer0_base + GPT_SRELOAD) = 0;
	*(timer_common.timer0_base + GPT_SCALER) = 0;
	hal_cpuDataStoreBarrier();
	*(timer_common.timer0_base + GPT_TRLDVAL((s32)timer_common.wdog)) = 1;
	hal_cpuDataStoreBarrier();

	/* Interrupt must be enabled for the watchdog to work */
	*(timer_common.timer0_base + GPT_TCTRL((s32)timer_common.wdog)) = TIMER_LOAD | TIMER_INT_ENABLE | TIMER_ENABLE;

	__builtin_unreachable();
}


int hal_timerRegister(intrFn_t f, void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = TIMER0_2_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


char *hal_timerFeatures(char *features, unsigned int len)
{
	(void)hal_strncpy(features, "Using General Purpose Timer", len);
	features[len - 1U] = '\0';
	return features;
}


void _hal_timerInit(u32 interval)
{
	u32 st, prescaler;

	timer_common.jiffies = 0;

	timer_common.timer0_base = _pmap_halMapDevice(PAGE_ALIGN(GPTIMER0_BASE), PAGE_OFFS(GPTIMER0_BASE), SIZE_PAGE);
	timer_common.wdog = *(timer_common.timer0_base + GPT_CONFIG) & 0x7U;

	/* Disable timer interrupts - bits cleared when written 1 */
	st = *(timer_common.timer0_base + GPT_TCTRL(TIMER_TIMEBASE)) & (TIMER_INT_ENABLE | TIMER_INT_PENDING);
	*(timer_common.timer0_base + GPT_TCTRL(TIMER_TIMEBASE)) = st;
	/* Disable timers */
	*(timer_common.timer0_base + GPT_TCTRL(TIMER_TIMEBASE)) = 0;
	*(timer_common.timer0_base + GPT_TCTRL(TIMER_WAKEUP)) = 0;

	/* Set prescaler for 1 MHz timer tick */
	prescaler = (u32)SYSCLK_FREQ / 1000000U;
	*(timer_common.timer0_base + GPT_SRELOAD) = prescaler - 1U;

	timer_setReloadValue(TIMER_TIMEBASE, TIMEBASE_INTERVAL);

	timer_common.ticksPerFreq = (u64)TIMEBASE_INTERVAL + 1UL;

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.timebaseHandler.f = _timer_irqHandler;
	timer_common.timebaseHandler.n = TIMER0_1_IRQ;
	timer_common.timebaseHandler.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.timebaseHandler);

	timer_common.wakeupHandler.f = _timer_irqHandler;
	timer_common.wakeupHandler.n = TIMER0_2_IRQ;
	timer_common.wakeupHandler.data = NULL;
	(void)hal_interruptsSetHandler(&timer_common.wakeupHandler);

	/* Enable timer and interrupts */
	/* Load reload value into counter register */
	*(timer_common.timer0_base + GPT_TCTRL(TIMER_TIMEBASE)) = TIMER_ENABLE | TIMER_INT_ENABLE | TIMER_LOAD | TIMER_PERIODIC;
}
