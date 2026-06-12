/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception and interrupt handling
 *
 * Copyright 2014, 2018, 2020 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/cpu.h"
#include "hal/interrupts.h"
#include "hal/list.h"
#include "config.h"

#include "proc/userintr.h"
#include "perf/trace-events.h"

#define SIZE_INTERRUPTS 159U

/* clang-format off */
enum { /* 1024 reserved */ ctlr = 0x400, typer, iidr, /* 29 reserved */ igroupr0 = 0x420, /* 16 registers */
	/* 16 reserved */ isenabler0 = 0x440, /* 16 registers */ /* 16 reserved */ icenabler0 = 0x460, /* 16 registers */
	/* 16 reserved */ ispendr0 = 0x480, /* 16 registers */ /* 16 reserved */ icpendr0 = 0x4a0, /* 16 registers */
	/* 16 reserved */ isactiver0 = 0x4c0, /* 16 registers */ /* 16 reserved */ icactiver0 = 0x4e0, /* 16 registers */
	/* 16 reserved */ ipriorityr0 = 0x500, /* 64 registers */ /* 128 reserved */ itargetsr0 = 0x600, /* 64 registers */
	/* 128 reserved */ icfgr0 = 0x700, /* 32 registers */ /* 32 reserved */ ppisr = 0x740, spisr0, /* 15 registers */
	/* 112 reserved */ sgir = 0x7c0, /* 3 reserved */ cpendsgir = 0x7c4, /* 4 registers */ spendsgir = 0x7c8, /* 4 registers */
	/* 40 reserved */ pidr4 = 0x7f4, pidr5, pidr6, pidr7, pidr0, pidr1, pidr2, pidr3, cidr0, cidr1, cidr2, cidr3,
	cctlr = 0x800, pmr, bpr, iar, eoir, rpr, hppir, abpr, aiar, aeoir, ahppir /* 41 reserved */, apr0 = 0x834, /* 3 reserved */
	nsapr0 = 0x838, /* 6 reserved */ ciidr = 0x83f, /* 960 reserved */ dir = 0xc00 };
/* clang-format on */


static struct {
	volatile u32 *gic;
	spinlock_t spinlock[SIZE_INTERRUPTS];
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
	int trace_irqs;
} interrupts;


int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);

/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Provided by toolchain" */
extern unsigned int _end;


/* parasoft-suppress-next-line MISRAC2012-RULE_2_2 MISRAC2012-RULE_8_4 "Function is used externally within assembler code" */
int interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0;
	spinlock_ctx_t sc;
	int trace;

	u32 iarValue = *(interrupts.gic + iar);
	n = iarValue & 0x3ffU;

	if (n >= SIZE_INTERRUPTS) {
		return 0;
	}

	trace = (interrupts.trace_irqs != 0 && n != TIMER_IRQ_ID) ? 1 : 0;
	if (trace != 0) {
		trace_eventInterruptEnter(n);
	}

	hal_spinlockSet(&interrupts.spinlock[n], &sc);

	interrupts.counters[n]++;

	h = interrupts.handlers[n];
	if (h != NULL) {
		do {
			if (h->f(n, ctx, h->data) != 0) {
				reschedule = 1;
			}
			h = h->next;
		} while (h != interrupts.handlers[n]);
	}

	if (reschedule != 0) {
		(void)threads_schedule(n, ctx, NULL);
	}

	*(interrupts.gic + eoir) = iarValue;

	hal_spinlockClear(&interrupts.spinlock[n], &sc);

	if (trace != 0) {
		trace_eventInterruptExit(n);
	}

	return reschedule;
}


static void interrupts_enableIRQ(unsigned int irqn)
{
	*(interrupts.gic + isenabler0 + (irqn >> 5)) = 1UL << (irqn & 0x1fU);
}


static void interrupts_disableIRQ(unsigned int irqn)
{
	*(interrupts.gic + icenabler0 + (irqn >> 5)) = 1UL << (irqn & 0x1fU);
}


static void interrupts_setConf(unsigned int irqn, u32 conf)
{
	u32 t;

	t = *(interrupts.gic + icfgr0 + (irqn >> 4)) & ~(0x3U << ((irqn & 0xfU) << 1));
	*(interrupts.gic + icfgr0 + (irqn >> 4)) = t | ((conf & 0x3U) << ((irqn & 0xfU) << 1));
}


static void interrupts_setPriority(unsigned int irqn, u32 priority)
{
	u32 mask = *(interrupts.gic + ipriorityr0 + (irqn >> 2)) & ~(0xffU << ((irqn & 0x3U) << 3));

	*(interrupts.gic + ipriorityr0 + (irqn >> 2)) = mask | ((priority & 0xffU) << ((irqn & 0x3U) << 3));
}


static inline u32 interrupts_getPriority(unsigned int irqn)
{
	return (*(interrupts.gic + ipriorityr0 + (irqn >> 2)) >> ((irqn & 0x3U) << 3)) & 0xffU;
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS) {
		return -1;
	}

	hal_spinlockSet(&interrupts.spinlock[h->n], &sc);
	HAL_LIST_ADD(&interrupts.handlers[h->n], h);

	interrupts_setPriority(h->n, h->n >> 5);
	interrupts_setConf(h->n, 0x3);
	interrupts_enableIRQ(h->n);

	hal_spinlockClear(&interrupts.spinlock[h->n], &sc);

	return 0;
}


char *hal_interruptsFeatures(char *features, size_t len)
{
	(void)hal_strncpy(features, "Using GIC interrupt controller", len);
	features[len - 1U] = '\0';

	return features;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS) {
		return -1;
	}

	hal_spinlockSet(&interrupts.spinlock[h->n], &sc);
	HAL_LIST_REMOVE(&interrupts.handlers[h->n], h);

	if (interrupts.handlers[h->n] == NULL) {
		interrupts_disableIRQ(h->n);
	}

	hal_spinlockClear(&interrupts.spinlock[h->n], &sc);

	return 0;
}


void _hal_interruptsTrace(int enable)
{
	interrupts.trace_irqs = enable;
}


void _hal_interruptsInit(void)
{
	u32 i, t, priority;

	interrupts.trace_irqs = 0;

	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		interrupts.handlers[i] = NULL;
		interrupts.counters[i] = 0;
		hal_spinlockCreate(&interrupts.spinlock[i], "interrupts");
	}

	interrupts.gic = (void *)(((u32)&_end + (5U * SIZE_PAGE) - 1U) & ~(SIZE_PAGE - 1U));

	*(interrupts.gic + ctlr) &= ~1U;

	interrupts_setPriority(0, 0xff);
	priority = interrupts_getPriority(0);

	for (i = 32; i <= SIZE_INTERRUPTS; ++i) {
		*(interrupts.gic + icenabler0 + (i >> 5)) = 1UL << (i & 0x1fU);
		interrupts_setConf(i, 0);
		interrupts_setPriority(i, priority >> 1);
		t = *(interrupts.gic + itargetsr0 + (i >> 2)) & ~(0xffU << ((i & 0x3U) << 3));
		*(interrupts.gic + itargetsr0 + (i >> 2)) = t | (1UL << ((i & 0x3U) << 3));
		*(interrupts.gic + igroupr0 + (i >> 5)) &= ~(1U << (i & 0x1fU));
	}

	*(interrupts.gic + ctlr) |= 1U;
	*(interrupts.gic + cctlr) &= ~1U;

	for (i = 0; i < 32U; ++i) {
		if (i > 15U) {
			interrupts_setConf(i, 0);
		}
		*(interrupts.gic + icenabler0) = 1UL << i;
		interrupts_setPriority(i, priority >> 1);
		*(interrupts.gic + igroupr0) &= ~(1U << i);
	}

	*(interrupts.gic + cctlr) |= 1U;
	*(interrupts.gic + bpr) = 0U;
	*(interrupts.gic + pmr) = 0xffU;
}


void hal_cpuBroadcastIPI(unsigned int intr)
{
}
