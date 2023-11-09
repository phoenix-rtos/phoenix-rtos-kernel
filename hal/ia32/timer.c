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

#define LAPIC_TIMER_ONE_SHOT        0
#define LAPIC_TIMER_DEFAULT_DIVIDER 3 /* 3 means 8 (1 << 3) */

/* 64 bit registers, access must be aligned
   Trying to use exclusive-access mechanisms (ex. lock mov, xchg, etc.) is undefined */
#define HPET_ID         0x00
#define HPET_CONFIG     0x10
#define HPET_IRQ_STATUS 0x20
#define HPET_COUNTER    0xf0

#define HPET_ID_LEGACY_CAPABLE           (1u << 15)
#define HPET_LEGACY_TMR1_IRQ             8
#define HPET_CONFIG_TMR_IRQ_EN           (1u << 2)
#define HPET_CONFIG_TMR_PERIODIC         (1u << 3)
#define HPET_CONFIG_TMR_CAN_BE_PERIODIC  (1u << 4)
#define HPET_CONFIG_TMR_PERIODIC_CAN_SET (1u << 6)
#define HPET_CONFIG_TMR_32BIT_MODE       (1u << 8)

typedef enum { timer_unknown, timer_pit, timer_lapic, timer_hpet } timerType_t;

struct {
	intr_handler_t handler;
	spinlock_t sp;
	u32 intervalUs;

	timerType_t schedulerTimerType;
	timerType_t timestampTimerType;
	union {
		struct {
			volatile time_t jiffies;
		} pit;
		struct {
			volatile u64 cycles; /* How many ticks were there on CPU0 */
		} lapic;
		struct {
			hal_gasMapped_t addr;
			u32 period;
			intr_handler_t tmr1;
		} hpet;
	} timestampTimer;
	union {
		struct {
			u32 frequency;
			u32 wait[MAX_CPU_COUNT]; /* Wait time (in cycles) of this CPU */
		} lapic;
	} schedulerTimer;
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

	(void)hal_cpuAtomAdd((volatile u32 *)&timer.timestampTimer.pit.jiffies, timer.intervalUs);
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

	timer.timestampTimerType = timer_pit;
	timer.schedulerTimerType = timer_pit;

	timer.timestampTimer.pit.jiffies = 0;

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
	return (u32)(((cycles << LAPIC_TIMER_DEFAULT_DIVIDER) * 1000) / (u64)timer.schedulerTimer.lapic.frequency);
}


static inline u64 _hal_lapicTimerUs2Cyc(u32 us)
{
	return (((u64)us) * (u64)timer.schedulerTimer.lapic.frequency) / (1000 << LAPIC_TIMER_DEFAULT_DIVIDER);
}


static int hal_lapicTimerIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	spinlock_ctx_t sc;
	const unsigned int id = hal_cpuGetID();
	hal_spinlockSet(&timer.sp, &sc);
	if ((timer.timestampTimerType == timer_lapic) && (id == 0)) {
		timer.timestampTimer.lapic.cycles += timer.schedulerTimer.lapic.wait[id];
	}
	timer.schedulerTimer.lapic.wait[id] = _hal_lapicTimerUs2Cyc(timer.intervalUs);
	_hal_lapicTimerStart(timer.schedulerTimer.lapic.wait[id]);
	hal_spinlockClear(&timer.sp, &sc);
	return 0;
}


/* High Precision Event Timers */


static inline u32 _hal_hpetRead(u32 offset)
{
	u32 ret;
	(void)_hal_gasRead32(&timer.timestampTimer.hpet.addr, offset, &ret);
	return ret;
}


static inline void _hal_hpetWrite(u32 offset, u32 val)
{
	(void)_hal_gasWrite32(&timer.timestampTimer.hpet.addr, offset, val);
}


/* 0 disables, everything else enables */
static inline void _hal_hpetEnable(int val)
{
	_hal_hpetWrite(HPET_CONFIG, (_hal_hpetRead(HPET_CONFIG) & 0x3) | (val != 0 ? 1 : 0));
}


static inline u64 _hal_hpetGetCounter(void)
{
	u32 high, low;
	do {
		high = _hal_hpetRead(HPET_COUNTER + sizeof(u32));
		low = _hal_hpetRead(HPET_COUNTER);
	} while (high != _hal_hpetRead(HPET_COUNTER + sizeof(u32)));
	return ((u64)high) << 32 | low;
}


static inline void _hal_hpetSetCounter(u64 val)
{
	u32 high, low;
	low = (u32)val;
	high = (u32)(val >> 32);
	_hal_hpetWrite(HPET_COUNTER, low);
	_hal_hpetWrite(HPET_COUNTER + sizeof(u32), high);
}


static time_t _hal_hpetGetUs(void)
{
	u64 ret = _hal_hpetGetCounter();
	ret *= (u64)timer.timestampTimer.hpet.period;
	ret /= 1000000000LLU;
	return ret;
}


static int _hal_hpetInit(void)
{
	if (hal_config.hpet == NULL) {
		return -1;
	}
	_hal_gasAllocDevice(&hal_config.hpet->baseAddress, &timer.timestampTimer.hpet.addr, 0x400);
	if (_hal_gasRead32(&timer.timestampTimer.hpet.addr, HPET_ID + sizeof(u32), &timer.timestampTimer.hpet.period) != 0) {
		return -1;
	}
	_hal_hpetSetCounter(0LLU);
	timer.timestampTimerType = timer_hpet;
	_hal_hpetEnable(1);
	return 0;
}


static int _hal_lapicTimerInit(u32 intervalUs)
{
	u64 freq, hpetDelta;
	time_t hpetStart, hpetEnd;
	u32 lapicDelta;
	u16 pitDelta;
	if (hal_isLapicPresent() == 0) {
		return -1;
	}
	_hal_lapicTimerConfigure(LAPIC_TIMER_ONE_SHOT, 0, SYSTICK_IRQ + INTERRUPTS_VECTOR_OFFSET);
	_hal_lapicTimerSetDivider(LAPIC_TIMER_DEFAULT_DIVIDER);
	lapicDelta = 0xffffffffu;
	switch (timer.timestampTimerType) {
		case timer_pit:
			pitDelta = 0xffff;
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
			timer.timestampTimerType = timer_lapic;
			timer.timestampTimer.lapic.cycles = 0;
			break;
		case timer_hpet:
			_hal_pitSetTimer(0, PIT_OPERATING_ONE_SHOT); /* Disable PIT */
			hpetStart = _hal_hpetGetUs();
			_hal_lapicTimerStart(lapicDelta);
			do {
				hpetEnd = _hal_hpetGetUs();
			} while (hpetEnd - hpetStart < 100000); /* 100ms */
			lapicDelta -= _hal_lapicTimerGetCounter();
			_hal_lapicTimerStop();
			hpetDelta = hpetEnd - hpetStart;

			freq = (((u64)lapicDelta) * 1000) << LAPIC_TIMER_DEFAULT_DIVIDER;
			freq /= hpetDelta;
			break;
		default:
			return -1;
	}
	timer.schedulerTimerType = timer_lapic;
	timer.intervalUs = intervalUs;

	timer.schedulerTimer.lapic.frequency = freq;

	(void)hal_timerRegister(hal_lapicTimerIrqHandler, NULL, &timer.handler);
	return 0;
}


void hal_timerInitCore(const unsigned int id)
{
	switch (timer.schedulerTimerType) {
		case timer_lapic:
			_hal_lapicTimerConfigure(LAPIC_TIMER_ONE_SHOT, 0, SYSTICK_IRQ + INTERRUPTS_VECTOR_OFFSET);
			_hal_lapicTimerSetDivider(LAPIC_TIMER_DEFAULT_DIVIDER);
			timer.schedulerTimer.lapic.wait[id] = 1;
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
	switch (timer.timestampTimerType) {
		case timer_lapic:
			ret = _hal_lapicTimerCyc2Us(timer.timestampTimer.lapic.cycles);
			break;
		case timer_pit:
			ret = timer.timestampTimer.pit.jiffies;
			break;
		case timer_hpet:
			ret = _hal_hpetGetUs();
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
	switch (timer.schedulerTimerType) {
		case timer_lapic:
			id = hal_cpuGetID();
			if ((timer.timestampTimerType == timer_lapic) && (id == 0)) {
				timer.timestampTimer.lapic.cycles += (timer.schedulerTimer.lapic.wait[id] - _hal_lapicTimerGetCounter());
			}
			timer.schedulerTimer.lapic.wait[id] = _hal_lapicTimerUs2Cyc(waitUs);
			_hal_lapicTimerStart(timer.schedulerTimer.lapic.wait[id]);
			break;
		default:
			/* Not supported */
			break;
	}
	hal_spinlockClear(&timer.sp, &sc);
}


void _hal_timerInit(u32 intervalUs)
{
	timer.schedulerTimerType = timer_unknown;
	timer.timestampTimerType = timer_pit;

	hal_spinlockCreate(&timer.sp, "timer");

	(void)_hal_hpetInit();

	if (_hal_lapicTimerInit(intervalUs) != 0) {
		_hal_pitInit(intervalUs);
	}
}
