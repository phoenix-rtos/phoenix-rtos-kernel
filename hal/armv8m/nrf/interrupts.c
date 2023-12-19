/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling
 *
 * Copyright 2017, 2020, 2022 Phoenix Systems
 * Author: Pawel Pisarczyk, Hubert Buczynski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/cpu.h"
#include "hal/list.h"
#include "hal/armv8m/armv8m.h"
#include "hal/armv8m/nrf/91/nrf91.h"

#include "proc/userintr.h"

#include "config.h"

#ifdef CPU_NRF91
/* Based on INTLINESNUM value (ICTR cpu register) */
#define SIZE_INTERRUPTS 256
#endif

/* Value based on other target architectures */
#define SIZE_HANDLERS 4


static struct {
	volatile u32 *nvic;
	volatile u32 *scb;
	spinlock_t spinlock;
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts;


/* clang-format off */
enum { nvic_iser = 0, nvic_icer = 32, nvic_ispr = 64, nvic_icpr = 96, nvic_iabr = 128,
	nvic_ip = 192 };
/* clang-format on */


extern int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


void _interrupts_nvicSetIRQ(s8 irqn, u8 state)
{
	volatile u32 *ptr = interrupts.nvic + ((u8)irqn >> 5) + ((state != 0) ? nvic_iser : nvic_icer);
	*ptr = 1u << (irqn & 0x1f);

	hal_cpuDataSyncBarrier();
	hal_cpuInstrBarrier();
}


void _interrupts_nvicSetPriority(s8 irqn, u32 priority)
{
	volatile u32 *ptr;

	ptr = ((u32 *)(interrupts.nvic + nvic_ip)) + (irqn / 4);

	/* We set only group priority field */
	*ptr = (priority << (8 * (irqn % 4) + 4));
}


void _interrupts_nvicSystemReset(void)
{
	*(interrupts.scb + scb_aircr) = ((0x5fau << 16) | (*(interrupts.scb + scb_aircr) & (0x700u)) | (1u << 2));

	hal_cpuDataSyncBarrier();

	for (;;) {
	}
}


void interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0;
	spinlock_ctx_t sc;

	if (n >= SIZE_INTERRUPTS) {
		return;
	}

	hal_spinlockSet(&interrupts.spinlock, &sc);

	interrupts.counters[n]++;

	h = interrupts.handlers[n];
	if (h != NULL) {
		do {
			hal_cpuSetGot(h->got);
			if (h->f(n, ctx, h->data) != 0) {
				reschedule = 1;
			}
			h = h->next;
		} while (h != interrupts.handlers[n]);
	}

	hal_spinlockClear(&interrupts.spinlock, &sc);

	if (reschedule != 0) {
		threads_schedule(n, ctx, NULL);
	}
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS) {
		return -1;
	}

	hal_spinlockSet(&interrupts.spinlock, &sc);
	h->got = hal_cpuGetGot();

	/* adding to interrupt handlers tree */
	HAL_LIST_ADD(&interrupts.handlers[h->n], h);

	if (h->n >= 0x10) {
		_interrupts_nvicSetPriority(h->n - 0x10, 1);
		_interrupts_nvicSetIRQ(h->n - 0x10, 1);
	}
	hal_spinlockClear(&interrupts.spinlock, &sc);

	return 0;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS) {
		return -1;
	}

	hal_spinlockSet(&interrupts.spinlock, &sc);
	HAL_LIST_REMOVE(&interrupts.handlers[h->n], h);

	if (h->n >= 0x10 && interrupts.handlers[h->n] == NULL) {
		_interrupts_nvicSetIRQ(h->n - 0x10, 0);
	}

	hal_spinlockClear(&interrupts.spinlock, &sc);

	return 0;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using NVIC interrupt controller", len);
	features[len - 1] = 0;

	return features;
}


__attribute__((section(".init"))) void _hal_interruptsInit(void)
{
	unsigned int n;

	for (n = 0; n < SIZE_INTERRUPTS; ++n) {
		interrupts.handlers[n] = NULL;
		interrupts.counters[n] = 0;
	}

	interrupts.nvic = (void *)0xe000e100;
	interrupts.scb = (void *)0xe000e000;

	hal_spinlockCreate(&interrupts.spinlock, "interrupts.spinlock");

	_nrf91_scbSetPriority(SYSTICK_IRQ, 1);
	_nrf91_scbSetPriority(PENDSV_IRQ, 1);
	_nrf91_scbSetPriority(SVC_IRQ, 0);

	/* Set no subprorities in Interrupt Group Priority */
	_nrf91_scbSetPriorityGrouping(3);

	return;
}
