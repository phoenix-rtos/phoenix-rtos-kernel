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

#include "proc/userintr.h"

#define SIZE_INTERRUPTS 159
#define SIZE_HANDLERS   4


#define _intr_add(list, t) \
	do { \
		if (t == NULL) \
			break; \
		if (*list == NULL) { \
			t->next = t; \
			t->prev = t; \
			(*list) = t; \
			break; \
		} \
		t->prev = (*list)->prev; \
		(*list)->prev->next = t; \
		t->next = (*list); \
		(*list)->prev = t; \
	} while (0)


#define _intr_remove(list, t) \
	do { \
		if (t == NULL) \
			break; \
		if ((t->next == t) && (t->prev == t)) \
			(*list) = NULL; \
		else { \
			t->prev->next = t->next; \
			t->next->prev = t->prev; \
			if (t == (*list)) \
				(*list) = t->next; \
		} \
		t->next = NULL; \
		t->prev = NULL; \
	} while (0)


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


struct {
	volatile u32 *gic;
	spinlock_t spinlock[SIZE_INTERRUPTS];
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts;


extern int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);

extern unsigned int _end;


void interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0;
	spinlock_ctx_t sc;

	n = *(interrupts.gic + iar) & 0x3ff;

	if (n >= SIZE_INTERRUPTS)
		return;

	hal_spinlockSet(&interrupts.spinlock[n], &sc);

	interrupts.counters[n]++;

	if ((h = interrupts.handlers[n]) != NULL) {
		do
			reschedule |= h->f(n, ctx, h->data);
		while ((h = h->next) != interrupts.handlers[n]);
	}

	if (reschedule)
		threads_schedule(n, ctx, NULL);

	*(interrupts.gic + eoir) = (*(interrupts.gic + eoir) & ~0x3ff) | n;

	hal_spinlockClear(&interrupts.spinlock[n], &sc);

	return;
}


static void interrupts_enableIRQ(unsigned int irqn)
{
	*(interrupts.gic + isenabler0 + (irqn >> 5)) = 1 << (irqn & 0x1f);
}


static void interrupts_disableIRQ(unsigned int irqn)
{
	*(interrupts.gic + icenabler0 + (irqn >> 5)) = 1 << (irqn & 0x1f);
}


static void interrupts_setConf(unsigned int irqn, u32 conf)
{
	u32 t;

	t = *(interrupts.gic + icfgr0 + (irqn >> 4)) & ~(0x3 << ((irqn & 0xf) << 1));
	*(interrupts.gic + icfgr0 + (irqn >> 4)) = t | ((conf & 0x3) << ((irqn & 0xf) << 1));
}


static void interrupts_setPriority(unsigned int irqn, u32 priority)
{
	u32 mask = *(interrupts.gic + ipriorityr0 + (irqn >> 2)) & ~(0xff << ((irqn & 0x3) << 3));

	*(interrupts.gic + ipriorityr0 + (irqn >> 2)) = mask | ((priority & 0xff) << ((irqn & 0x3) << 3));
}


static inline u32 interrupts_getPriority(unsigned int irqn)
{
	return (*(interrupts.gic + ipriorityr0 + (irqn >> 2)) >> ((irqn & 0x3) << 3)) & 0xff;
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS)
		return -1;

	hal_spinlockSet(&interrupts.spinlock[h->n], &sc);
	_intr_add(&interrupts.handlers[h->n], h);

	interrupts_setPriority(h->n, h->n >> 5);
	interrupts_setConf(h->n, 0x3);
	interrupts_enableIRQ(h->n);

	hal_spinlockClear(&interrupts.spinlock[h->n], &sc);

	return 0;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using GIC interrupt controller", len);
	features[len - 1] = 0;

	return features;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS)
		return -1;

	hal_spinlockSet(&interrupts.spinlock[h->n], &sc);
	_intr_remove(&interrupts.handlers[h->n], h);

	if (interrupts.handlers[h->n] == NULL)
		interrupts_disableIRQ(h->n);

	hal_spinlockClear(&interrupts.spinlock[h->n], &sc);

	return 0;
}


void _hal_interruptsInit(void)
{
	u32 i, t, priority;

	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		interrupts.handlers[i] = NULL;
		interrupts.counters[i] = 0;
		hal_spinlockCreate(&interrupts.spinlock[i], "interrupts");
	}

	interrupts.gic = (void *)(((u32)&_end + (5 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));

	*(interrupts.gic + ctlr) &= ~1;

	interrupts_setPriority(0, 0xff);
	priority = interrupts_getPriority(0);

	for (i = 32; i <= SIZE_INTERRUPTS; ++i) {
		*(interrupts.gic + icenabler0 + (i >> 5)) = 1 << (i & 0x1f);
		interrupts_setConf(i, 0);
		interrupts_setPriority(i, priority >> 1);
		t = *(interrupts.gic + itargetsr0 + (i >> 2)) & ~(0xff << ((i & 0x3) << 3));
		*(interrupts.gic + itargetsr0 + (i >> 2)) = t | (1 << ((i & 0x3) << 3));
		*(interrupts.gic + igroupr0 + (i >> 5)) &= ~(1 << (i & 0x1f));
	}

	*(interrupts.gic + ctlr) |= 1;
	*(interrupts.gic + cctlr) &= ~1;

	for (i = 0; i < 32; ++i) {
		if (i > 15)
			interrupts_setConf(i, 0);
		*(interrupts.gic + icenabler0) = 1 << i;
		interrupts_setPriority(i, priority >> 1);
		*(interrupts.gic + igroupr0) &= ~(1 << i);
	}

	*(interrupts.gic + cctlr) |= 1;
	*(interrupts.gic + bpr) = 0;
	*(interrupts.gic + pmr) = 0xff;
}
