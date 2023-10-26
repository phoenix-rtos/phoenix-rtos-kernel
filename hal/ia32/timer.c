/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System timer driver
 *
 * Copyright 2012, 2016, 2023 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Andrzej Stalke
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/timer.h"
#include "hal/cpu.h"
#include "init.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "ia32.h"

#define PIT_FREQUENCY 1193 /* kHz */

#define PIT_BCD                0
#define PIT_CHANNEL_0          (0 << 6)
#define PIT_CHANNEL_1          (1 << 6)
#define PIT_CHANNEL_2          (2 << 6)
#define PIT_ACCESS_BOTH        (3 << 4)
#define PIT_OPERATING_ONE_SHOT (0 << 1)
#define PIT_OPERATING_RATE_GEN (2 << 1)

#define LAPIC_TIMER_ONE_SHOT 0
#define LAPIC_TIMER_DEFAULT_DIVIDER 3 /* 3 means 8 (1 << 3) */

struct {
	intr_handler_t handler;
	spinlock_t sp;
	u32 intervalUs;

	enum { timer_unknown,
		timer_pit,
		timer_lapic } timerType;
	union {
		struct {
			volatile time_t jiffies;
		} pit;
		struct {
			u32 frequency;
			struct {
				u64 cycles; /* How many ticks were there */
				u32 wait;   /* Wait time (in cycles) of this CPU */
			} local[MAX_CPU_COUNT];
		} lapic;
	};
} timer;


int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = SYSTICK_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}

/* Programmable Interval Timer (Intel 8253/8254) */


static int hal_pitTimerIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	timer.pit.jiffies += timer.intervalUs;
	return 0;
}


static inline u16 _hal_pitCalculateDivider(u32 intervalUs)
{
	u32 tmp;
	tmp = intervalUs;
	tmp *= PIT_FREQUENCY;
	tmp /= 1000;
	if (tmp >= 65536) {
		tmp = 0;
	}
	return (u16)tmp;
}


static inline void _hal_pitSetTimer(u16 reloadValue, u8 opMode)
{
	/* First generator, operation - CE write, work mode 2, binary counting */
	hal_outb(PORT_PIT_COMMAND, PIT_CHANNEL_0 | PIT_ACCESS_BOTH | opMode);
	hal_outb(PORT_PIT_DATA_CHANNEL0, (u8)(reloadValue & 0xff));
	hal_outb(PORT_PIT_DATA_CHANNEL0, (u8)(reloadValue >> 8));
}


static inline u16 _hal_pitReadTimer(void)
{
	u16 low, high;
	hal_outb(PORT_PIT_COMMAND, PIT_CHANNEL_0); /* Latch command */
	low = hal_inb(PORT_PIT_DATA_CHANNEL0);
	high = hal_inb(PORT_PIT_DATA_CHANNEL0);
	return (high << 8) | low;
}


static void _hal_pitInit(u32 intervalUs)
{
	intervalUs /= hal_cpuGetCount();
	timer.intervalUs = intervalUs;

	_hal_pitSetTimer(_hal_pitCalculateDivider(intervalUs), PIT_OPERATING_RATE_GEN);

	timer.timerType = timer_pit;

	timer.pit.jiffies = 0;

	(void)hal_timerRegister(hal_pitTimerIrqHandler, NULL, &timer.handler);
}


/* Local APIC Timer */


static inline void _hal_lapicTimerSetDivider(u8 divider)
{
	/* Divider is a power of 2 */
	if (divider == 0) {
		/* Not recommended. It is claimed that it is bugged on some emulators */
		divider = 0xb;
	}
	else if (divider > 4) {
		divider += 3;
	}
	else {
		divider -= 1;
	}
	_hal_lapicWrite(LAPIC_LVT_TMR_DC_REG, divider);
}


static inline void _hal_lapicTimerStart(u32 counter)
{
	_hal_lapicWrite(LAPIC_LVT_TMR_IC_REG, counter);
}


static inline void _hal_lapicTimerStop(void)
{
	_hal_lapicWrite(LAPIC_LVT_TMR_IC_REG, 0);
}


static inline u32 _hal_lapicTimerGetCounter(void)
{
	return _hal_lapicRead(LAPIC_LVT_TMR_CC_REG);
}


static inline void _hal_lapicTimerConfigure(u32 mode, u32 mask, u32 vector)
{
	_hal_lapicWrite(LAPIC_LVT_TIMER_REG, (vector & 0xff) | ((mask & 0x1) << 16) | ((mode & 0x3) << 17));
}


static inline u32 _hal_lapicTimerCyc2Us(u64 cycles)
{
	return (u32)(((cycles << LAPIC_TIMER_DEFAULT_DIVIDER) * 1000) / (u64)timer.lapic.frequency);
}


static inline u64 _hal_lapicTimerUs2Cyc(u32 us)
{
	return (((u64)us) * (u64)timer.lapic.frequency) / (1000 << LAPIC_TIMER_DEFAULT_DIVIDER);
}


static int hal_lapicTimerIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	spinlock_ctx_t sc;
	const unsigned int id = hal_cpuGetID();
	hal_spinlockSet(&timer.sp, &sc);
	timer.lapic.local[id].cycles += timer.lapic.local[id].wait;
	timer.lapic.local[id].wait = _hal_lapicTimerUs2Cyc(timer.intervalUs);
	_hal_lapicTimerStart(timer.lapic.local[id].wait);
	hal_spinlockClear(&timer.sp, &sc);
	return 0;
}


static int _hal_lapicTimerInit(u32 intervalUs)
{
	u64 freq;
	u32 lapicDelta;
	u16 pitDelta;
	if (hal_isLapicPresent() == 1) {
		lapicDelta = 0xffffffffu;
		pitDelta = 0xffff;
		_hal_lapicTimerConfigure(LAPIC_TIMER_ONE_SHOT, 0, SYSTICK_IRQ + INTERRUPTS_VECTOR_OFFSET);
		_hal_lapicTimerSetDivider(LAPIC_TIMER_DEFAULT_DIVIDER);
		_hal_pitSetTimer(pitDelta, PIT_OPERATING_ONE_SHOT);
		_hal_lapicTimerStart(lapicDelta);
		while (pitDelta > 0x0fff) { /* Wait is around 51.500 ms*/
			pitDelta = _hal_pitReadTimer();
		}
		lapicDelta -= _hal_lapicTimerGetCounter();
		_hal_lapicTimerStop();
		pitDelta = 0xffff - pitDelta; /* timePassed = pitDelta / PIT_FREQUENCY */

		freq = ((u64)lapicDelta) * ((u64)PIT_FREQUENCY);
		freq <<= LAPIC_TIMER_DEFAULT_DIVIDER;
		freq /= (u64)pitDelta; /* Frequency in kHz, with current technology it should fit in 32 bit */

		timer.timerType = timer_lapic;
		timer.intervalUs = intervalUs;

		timer.lapic.frequency = freq;

		(void)hal_timerRegister(hal_lapicTimerIrqHandler, NULL, &timer.handler);
		return 0;
	}
	else {
		return 1;
	}
}


void hal_timerInitCore(const unsigned int id)
{
	switch (timer.timerType) {
		case timer_lapic:
			_hal_lapicTimerConfigure(LAPIC_TIMER_ONE_SHOT, 0, SYSTICK_IRQ + INTERRUPTS_VECTOR_OFFSET);
			_hal_lapicTimerSetDivider(LAPIC_TIMER_DEFAULT_DIVIDER);
			timer.lapic.local[id].cycles = 0;
			timer.lapic.local[id].wait = 1;
			_hal_lapicTimerStart(1);
			break;
		default:
			break;
	}
}


time_t hal_timerGetUs(void)
{
	spinlock_ctx_t sc;
	time_t ret;
	hal_spinlockSet(&timer.sp, &sc);
	switch (timer.timerType) {
		case timer_lapic:
			ret = _hal_lapicTimerCyc2Us(timer.lapic.local[0].cycles);
			break;
		case timer_pit:
			ret = timer.pit.jiffies;
			break;
		default:
			ret = -1;
			break;
	}
	hal_spinlockClear(&timer.sp, &sc);

	return ret;
}


void hal_timerSetWakeup(u32 waitUs)
{
	unsigned int id;
	spinlock_ctx_t sc;
	if (waitUs > timer.intervalUs) {
		waitUs = timer.intervalUs;
	}

	hal_spinlockSet(&timer.sp, &sc);
	switch (timer.timerType) {
		case timer_lapic:
			id = hal_cpuGetID();
			timer.lapic.local[id].cycles += (timer.lapic.local[id].wait - _hal_lapicTimerGetCounter());
			timer.lapic.local[id].wait = _hal_lapicTimerUs2Cyc(waitUs);
			_hal_lapicTimerStart(timer.lapic.local[id].wait);
			break;
		default:
			/* Not supported */
			break;
	}
	hal_spinlockClear(&timer.sp, &sc);
}


void _hal_timerInit(u32 intervalUs)
{
	timer.timerType = timer_unknown;
	hal_spinlockCreate(&timer.sp, "timer");

	if (_hal_lapicTimerInit(intervalUs) != 0) {
		_hal_pitInit(intervalUs);
	}
}
