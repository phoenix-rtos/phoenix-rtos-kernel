/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver (OMAP 5430)
 *
 * Copyright 2021, 2023, 2025 Phoenix Systems
 * Author: Hubert Buczynski, Aleksander Kaminski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/armv7r/armv7r.h"
#include "hal/timer.h"
#include "hal/spinlock.h"
#include "hal/string.h"

#include "tda4vm.h"

#define MCU_TIMER_BASE_ADDR(x) ((void *)(0x40400000 + ((x) * 0x10000)))
#define MCU_TIMER0_INTR        38
#define MCU_TIMER1_INTR        39
#define MCU_TIMER2_INTR        40
#define MCU_TIMER3_INTR        41
#define MCU_TIMER4_INTR        108
#define MCU_TIMER5_INTR        109
#define MCU_TIMER6_INTR        110
#define MCU_TIMER7_INTR        111
#define MCU_TIMER8_INTR        112
#define MCU_TIMER9_INTR        113

#define TIMER_SRC_FREQ_HZ 250000000
#define TIMER_TICK_HZ     1000

#define TIMER_INTR_OVERFLOW (1 << 1)
#define TIMER_MAX_COUNT     0xffffffffU

enum timer_regs {
	timer_tidr = (0x0 / 4),           /* Revision register */
	timer_tiocp_cfg = (0x10 / 4),     /* CBASS0 Configuration register */
	timer_irq_eoi = (0x20 / 4),       /* End-Of-Interrupt register */
	timer_irqstatus_raw = (0x24 / 4), /* Timer raw status register */
	timer_irqstatus = (0x28 / 4),     /* Timer status register */
	timer_irqstatus_set = (0x2c / 4), /* Interrupt enable register */
	timer_irqstatus_clr = (0x30 / 4), /* Interrupt disable register */
	timer_irqwakeen = (0x34 / 4),     /* Wake-up enable register */
	timer_tclr = (0x38 / 4),          /* Timer control register */
	timer_tcrr = (0x3c / 4),          /* Timer counter register */
	timer_tldr = (0x40 / 4),          /* Timer load register */
	timer_ttgr = (0x44 / 4),          /* Timer trigger register */
	timer_twps = (0x48 / 4),          /* Timer write-posted register */
	timer_tmar = (0x4c / 4),          /* Timer match register */
	timer_tcar1 = (0x50 / 4),         /* First captured value of the timer counter */
	timer_tsicr = (0x54 / 4),         /* Timer synchronous interface control register */
	timer_tcar2 = (0x58 / 4),         /* Second captured value of the timer counter */
	timer_tpir = (0x5c / 4),          /* Timer positive increment register */
	timer_tnir = (0x60 / 4),          /* Timer negative increment register */
	timer_tcvr = (0x64 / 4),          /* Timer CVR counter register */
	timer_tocr = (0x68 / 4),          /* Timer overflow counter register */
	timer_towr = (0x6c / 4),          /* Timer overflow wrapping register */
};


static struct {
	volatile u32 *base;
	intr_handler_t handler;
	volatile u64 jiffies;

	u32 ticksPerFreq;
	u32 reloadValue;
	spinlock_t sp;
} timer_common;


static int _timer_irqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;
	/* On TDA4VM timer interrupts are level triggered - don't use timer_irq_eoi */
	u32 st;
	spinlock_ctx_t sc;

	hal_spinlockSet(&timer_common.sp, &sc);

	st = *(timer_common.base + timer_irqstatus);
	/* Overflow IRQ */
	if ((st & TIMER_INTR_OVERFLOW) != 0) {
		timer_common.jiffies += timer_common.ticksPerFreq;
	}

	/* Clear IRQ status */
	*(timer_common.base + timer_irqstatus) = st;

	hal_spinlockClear(&timer_common.sp, &sc);

	hal_cpuDataSyncBarrier();

	return 0;
}


static time_t hal_timerCyc2us(u64 cyc)
{
	return cyc / ((u64)TIMER_SRC_FREQ_HZ / 1000000);
}


static u64 hal_timerGetCyc(void)
{
	spinlock_ctx_t sc;
	u64 jiffies;
	u32 cnt;

	hal_spinlockSet(&timer_common.sp, &sc);

	jiffies = timer_common.jiffies;
	cnt = *(timer_common.base + timer_tcrr);
	/* Check if there's pending jiffies increment */
	if ((*(timer_common.base + timer_irqstatus) & TIMER_INTR_OVERFLOW) != 0) {
		jiffies += timer_common.ticksPerFreq;
		timer_common.jiffies = jiffies;
		*(timer_common.base + timer_irqstatus) = TIMER_INTR_OVERFLOW;

		/* Timer might've just wrapped-around, take counter value again */
		cnt = *(timer_common.base + timer_tcrr);
	}

	cnt = cnt - timer_common.reloadValue;

	hal_spinlockClear(&timer_common.sp, &sc);

	return jiffies + cnt;
}


void hal_timerSetWakeup(u32 waitUs)
{
	/* Sleep mode not implemented on this platform */
}


static void timer_setPrescaler(u32 freq)
{
	u64 ticks;
	u32 prescaler;

	ticks = TIMER_SRC_FREQ_HZ / freq;

	prescaler = 0;
	while ((ticks >= TIMER_MAX_COUNT) && (prescaler < 8)) {
		prescaler++;
		ticks /= 2;
	}

	if (prescaler != 0) {
		/* Enable and set prescaler */
		prescaler--;
		*(timer_common.base + timer_tclr) |= (prescaler << 2);
		*(timer_common.base + timer_tclr) |= (1 << 5);
	}

	timer_common.ticksPerFreq = ticks;
	/* Timer will be reloaded with timer_tldr value on overflow, so subtract
	 * number of ticks per timer overflow from its maximum value */
	timer_common.reloadValue = (TIMER_MAX_COUNT - ticks) + 1;
}


time_t hal_timerGetUs(void)
{
	u64 ret = hal_timerGetCyc();

	return hal_timerCyc2us(ret);
}


int hal_timerRegister(intrFn_t f, void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = MCU_TIMER0_INTR;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


char *hal_timerFeatures(char *features, size_t len)
{
	hal_strncpy(features, "Using TI OMAP5430 Timer", len);
	features[len - 1] = '\0';
	return features;
}


static void timer_reset(void)
{
	/* Stop timer, set autoreload on, all other bits to 0 */
	*(timer_common.base + timer_tclr) = TIMER_INTR_OVERFLOW;
}


void _hal_timerInit(u32 interval)
{
	timer_common.base = MCU_TIMER_BASE_ADDR(0);
	timer_common.jiffies = 0;

	timer_reset();

	/* Trigger interrupt at TIMER_TICK_HZ = 1000 Hz */
	timer_setPrescaler(interval * hal_cpuGetCount());
	*(timer_common.base + timer_tldr) = timer_common.reloadValue;
	*(timer_common.base + timer_ttgr) = 0; /* Write any value to reload counter */

	hal_spinlockCreate(&timer_common.sp, "timer");
	timer_common.handler.f = _timer_irqHandler;
	timer_common.handler.n = MCU_TIMER0_INTR;
	timer_common.handler.data = NULL;
	hal_interruptsSetHandler(&timer_common.handler);

	/* Start counting */
	*(timer_common.base + timer_tclr) |= 1;

	/* Enable overflow IRQ */
	*(timer_common.base + timer_irqstatus_set) = TIMER_INTR_OVERFLOW;
}
