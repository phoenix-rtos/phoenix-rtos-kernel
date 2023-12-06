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
#include "hal/string.h"

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

typedef struct {
	enum { timer_undefined, timer_pit, timer_lapic, timer_hpet } type;
	unsigned int (*name)(char *s, unsigned int *len);
	int (*init)(u32 intervalUs);

	/* When used as scheduling timer */
	int (*schedulerIrq)(unsigned int, cpu_context_t *, void *);
	void (*schedulerSetWakeup)(u32 waitUs);
	void (*schedulerInitCore)(unsigned int id);

	/* When used as timestamp timer */
	time_t (*timestampGetUs)(void);
	time_t (*timestampBusyWaitUs)(time_t waitUs);
} hal_timer_t;


struct {
	intr_handler_t handler;
	spinlock_t sp;
	u32 intervalUs;

	const hal_timer_t *schedulerTimer;
	const hal_timer_t *timestampTimer;

	struct {
		volatile time_t jiffies;
	} pitData;

	struct {
		volatile u64 cycles; /* How many ticks were there on CPU0 */
		u32 frequency;
		u32 wait[MAX_CPU_COUNT]; /* Wait time (in cycles) of this CPU */
	} lapicData;

	struct {
		hal_gasMapped_t addr;
		u32 period;
		intr_handler_t tmr1;
	} hpetData;
} timer_common;


static unsigned int _hal_timersName(char *s, const char *prefix, unsigned long val, const char *suffix, unsigned int *len)
{
	unsigned int off = 0, n = hal_strlen(prefix) + sizeof(val) * 10 / 4;
	if (*len < n) {
		return off;
	}
	n = hal_i2s(prefix, s + off, val, 10, 0);
	off += n;
	*len -= n;

	hal_strncpy(s + off, suffix, *len);
	n = hal_strlen(suffix);
	if (*len < n) {
		n = *len;
	}
	off += n;
	*len -= n;
	return off;
}


/* Programmable Interval Timer (Intel 8253/8254) */

static unsigned int _hal_pitName(char *s, unsigned int *len)
{
	return _hal_timersName(s, "Programmable Interval Timer (", PIT_FREQUENCY, "kHz)", len);
}


static int _hal_pitTimerIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)arg;
	(void)ctx;

	(void)hal_cpuAtomAdd((volatile u32 *)&timer_common.pitData.jiffies, timer_common.intervalUs);
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


static time_t _hal_pitGetUs(void)
{
	return timer_common.pitData.jiffies;
}


static time_t _hal_pitBusyWaitUs(time_t waitUs)
{
	u64 sumTicks = 0;
	s64 ticks;
	u16 startPitDelta, pitDelta = 0;

	for (ticks = (((s64)PIT_FREQUENCY) * ((s64)waitUs)) / 1000; ticks > 0; ticks -= pitDelta) {
		if (ticks <= 0xf000) {
			startPitDelta = 0xfff + ticks;
		}
		else {
			startPitDelta = 0xffff;
		}
		pitDelta = startPitDelta;
		_hal_pitSetTimer(startPitDelta, PIT_OPERATING_ONE_SHOT);
		while (pitDelta > 0x0fff) {
			pitDelta = _hal_pitReadTimer();
		}
		pitDelta = startPitDelta - pitDelta;
		sumTicks += pitDelta;
	}
	return (sumTicks * 1000) / PIT_FREQUENCY;
}


static int _hal_defaultInit(u32 intervalUs)
{
	(void)intervalUs;
	_hal_pitSetTimer(0, PIT_OPERATING_ONE_SHOT); /* Disable PIT's regular IRQ */
	return 0;
}


static int _hal_pitInit(u32 intervalUs)
{
	intervalUs /= hal_cpuGetCount();
	timer_common.intervalUs = intervalUs;

	_hal_pitSetTimer(_hal_pitCalculateDivider(intervalUs), PIT_OPERATING_RATE_GEN);

	timer_common.pitData.jiffies = 0;
	return 0;
}


/* Local APIC Timer */


static unsigned int _hal_lapicTimerName(char *s, unsigned int *len)
{
	return _hal_timersName(s, "Local APIC Timer (", timer_common.lapicData.frequency, "kHz)", len);
}


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
	return (u32)(((cycles << LAPIC_TIMER_DEFAULT_DIVIDER) * 1000) / (u64)timer_common.lapicData.frequency);
}


static inline u64 _hal_lapicTimerUs2Cyc(u32 us)
{
	return (((u64)us) * (u64)timer_common.lapicData.frequency) / (1000 << LAPIC_TIMER_DEFAULT_DIVIDER);
}


static int _hal_lapicTimerIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	spinlock_ctx_t sc;
	const unsigned int id = hal_cpuGetID();
	hal_spinlockSet(&timer_common.sp, &sc);
	if ((timer_common.timestampTimer->type == timer_lapic) && (id == 0)) {
		timer_common.lapicData.cycles += timer_common.lapicData.wait[id];
	}
	timer_common.lapicData.wait[id] = _hal_lapicTimerUs2Cyc(timer_common.intervalUs);
	_hal_lapicTimerStart(timer_common.lapicData.wait[id]);
	hal_spinlockClear(&timer_common.sp, &sc);
	return 0;
}


/* High Precision Event Timers */


static unsigned int _hal_hpetName(char *s, unsigned int *len)
{
	return _hal_timersName(s, "High Precision Timer (", (u32)(1000000000000ULL / ((u64)timer_common.hpetData.period)), "kHz)", len);
}


static inline u32 _hal_hpetRead(u32 offset)
{
	u32 ret;
	(void)_hal_gasRead32(&timer_common.hpetData.addr, offset, &ret);
	return ret;
}


static inline void _hal_hpetWrite(u32 offset, u32 val)
{
	(void)_hal_gasWrite32(&timer_common.hpetData.addr, offset, val);
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
	ret *= (u64)timer_common.hpetData.period;
	ret /= 1000000000ULL;
	return ret;
}


static time_t _hal_hpetBusyWaitUs(time_t waitUs)
{
	time_t startUs, endUs;

	startUs = _hal_hpetGetUs();
	do {
		endUs = _hal_hpetGetUs();
	} while (endUs - startUs < waitUs);
	return endUs - startUs;
}


static int _hal_hpetInit(u32 intervalUs)
{
	(void)intervalUs;
	if (hal_config.hpet == NULL) {
		return -1;
	}
	_hal_gasAllocDevice(&hal_config.hpet->baseAddress, &timer_common.hpetData.addr, 0x400);
	if (_hal_gasRead32(&timer_common.hpetData.addr, HPET_ID + sizeof(u32), &timer_common.hpetData.period) != 0) {
		return -1;
	}
	_hal_hpetSetCounter(0ULL);
	_hal_hpetEnable(1);
	return 0;
}


static void _hal_lapicSetWakeup(u32 waitUs)
{
	const unsigned int id = hal_cpuGetID();
	if ((timer_common.timestampTimer->type == timer_lapic) && (id == 0)) {
		timer_common.lapicData.cycles += (timer_common.lapicData.wait[id] - _hal_lapicTimerGetCounter());
	}
	timer_common.lapicData.wait[id] = _hal_lapicTimerUs2Cyc(waitUs);
	_hal_lapicTimerStart(timer_common.lapicData.wait[id]);
}


static time_t _hal_lapicGetUs(void)
{
	return _hal_lapicTimerCyc2Us(timer_common.lapicData.cycles);
}


static void _hal_lapicInitCore(unsigned int id)
{
	_hal_lapicTimerConfigure(LAPIC_TIMER_ONE_SHOT, 0, SYSTICK_IRQ + INTERRUPTS_VECTOR_OFFSET);
	_hal_lapicTimerSetDivider(LAPIC_TIMER_DEFAULT_DIVIDER);
	timer_common.lapicData.wait[id] = 1;
	_hal_lapicTimerStart(1);
}


static int _hal_lapicTimerInit(u32 intervalUs)
{
	u64 freq, delta;
	u32 lapicDelta;
	if (hal_isLapicPresent() == 0) {
		return -1;
	}
	_hal_lapicTimerConfigure(LAPIC_TIMER_ONE_SHOT, 0, SYSTICK_IRQ + INTERRUPTS_VECTOR_OFFSET);
	_hal_lapicTimerSetDivider(LAPIC_TIMER_DEFAULT_DIVIDER);
	lapicDelta = 0xffffffffu;

	_hal_lapicTimerStart(lapicDelta);
	delta = timer_common.timestampTimer->timestampBusyWaitUs(100000);
	lapicDelta -= _hal_lapicTimerGetCounter();
	_hal_lapicTimerStop();

	freq = ((((u64)lapicDelta) * 1000) << LAPIC_TIMER_DEFAULT_DIVIDER) / delta;

	timer_common.intervalUs = intervalUs;

	timer_common.lapicData.frequency = freq;

	return 0;
}


void hal_timerInitCore(unsigned int id)
{
	if (timer_common.schedulerTimer->schedulerInitCore != NULL) {
		timer_common.schedulerTimer->schedulerInitCore(id);
	}
}


time_t hal_timerGetUs(void)
{
	spinlock_ctx_t sc;
	time_t ret;
	hal_spinlockSet(&timer_common.sp, &sc);
	ret = timer_common.timestampTimer->timestampGetUs();
	hal_spinlockClear(&timer_common.sp, &sc);

	return ret;
}


void hal_timerSetWakeup(u32 waitUs)
{
	spinlock_ctx_t sc;
	if (waitUs > timer_common.intervalUs) {
		waitUs = timer_common.intervalUs;
	}

	hal_spinlockSet(&timer_common.sp, &sc);
	if (timer_common.schedulerTimer->schedulerSetWakeup != NULL) {
		timer_common.schedulerTimer->schedulerSetWakeup(waitUs);
	}
	hal_spinlockClear(&timer_common.sp, &sc);
}


int hal_timerRegister(int (*f)(unsigned int, cpu_context_t *, void *), void *data, intr_handler_t *h)
{
	h->f = f;
	h->n = SYSTICK_IRQ;
	h->data = data;

	return hal_interruptsSetHandler(h);
}


char *hal_timerFeatures(char *features, unsigned int len)
{
	static const char textScheduling[] = "Scheduling timer: ";
	static const char textTimestamp[] = "\nhal: Timestamp timer: ";
	const size_t textSchedulingLength = sizeof(textScheduling) - 1;
	const size_t textTimestampLength = sizeof(textTimestamp) - 1;
	unsigned int off = 0, n;

	(void)hal_strncpy(features + off, textScheduling, len);
	n = (textSchedulingLength < len) ? textSchedulingLength : len;
	off += n;
	len -= n;

	off += timer_common.schedulerTimer->name(features + off, &len);

	(void)hal_strncpy(features + off, textTimestamp, len);
	n = (textTimestampLength < len) ? textTimestampLength : len;
	off += n;
	len -= n;

	off += timer_common.timestampTimer->name(features + off, &len);
	features[(len == 0) ? off - 1 : off] = '\0';
	return features;
}


static const hal_timer_t _hal_pitTimer = {
	.type = timer_pit,
	.name = _hal_pitName,
	.init = _hal_pitInit,
	.schedulerIrq = _hal_pitTimerIrqHandler,
	.schedulerSetWakeup = NULL,
	.schedulerInitCore = NULL,
	.timestampGetUs = _hal_pitGetUs,
	.timestampBusyWaitUs = _hal_pitBusyWaitUs,
};


/* Timer used at the very beginning, used only as timestamp for Local APIC Timer */
static const hal_timer_t _hal_defaultTimer = {
	.type = timer_undefined,
	.name = _hal_pitName,
	.init = _hal_defaultInit,
	.schedulerIrq = NULL,
	.schedulerSetWakeup = NULL,
	.schedulerInitCore = NULL,
	.timestampGetUs = _hal_pitGetUs,
	.timestampBusyWaitUs = _hal_pitBusyWaitUs,
};


static const hal_timer_t _hal_lapicTimer = {
	.type = timer_lapic,
	.name = _hal_lapicTimerName,
	.init = _hal_lapicTimerInit,
	.schedulerIrq = _hal_lapicTimerIrqHandler,
	.schedulerSetWakeup = _hal_lapicSetWakeup,
	.schedulerInitCore = _hal_lapicInitCore,
	.timestampGetUs = _hal_lapicGetUs,
	.timestampBusyWaitUs = NULL,
};


static const hal_timer_t _hal_hpetTimer = {
	.type = timer_hpet,
	.name = _hal_hpetName,
	.init = _hal_hpetInit,
	.schedulerIrq = NULL,
	.schedulerSetWakeup = NULL,
	.schedulerInitCore = NULL,
	.timestampGetUs = _hal_hpetGetUs,
	.timestampBusyWaitUs = _hal_hpetBusyWaitUs,
};


void _hal_timerInit(u32 intervalUs)
{
	_hal_defaultTimer.init(intervalUs);
	timer_common.schedulerTimer = &_hal_defaultTimer;
	timer_common.timestampTimer = &_hal_defaultTimer;

	hal_spinlockCreate(&timer_common.sp, "timer");

	if (_hal_hpetTimer.init(intervalUs) == 0) {
		timer_common.timestampTimer = &_hal_hpetTimer;
	}

	if (_hal_lapicTimer.init(intervalUs) == 0) {
		timer_common.schedulerTimer = &_hal_lapicTimer;
	}

	/* Edge cases */
	if (timer_common.timestampTimer->type == timer_undefined) {
		switch (timer_common.schedulerTimer->type) {
			case timer_lapic:
				/* LAPIC Timer can be used as a not very good timestamp (but better then PIT) */
				timer_common.timestampTimer = &_hal_lapicTimer;
				timer_common.lapicData.cycles = 0;
				break;
			default:
				/* Fallback to PIT (it must be used as both types of timers) */
				(void)_hal_pitInit(intervalUs);
				timer_common.schedulerTimer = &_hal_pitTimer;
				timer_common.timestampTimer = &_hal_pitTimer;
		}
	}

	(void)hal_timerRegister(timer_common.schedulerTimer->schedulerIrq, NULL, &timer_common.handler);
}
