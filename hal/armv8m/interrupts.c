/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling
 *
 * Copyright 2017, 2020, 2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Hubert Buczynski, Damian Loewnau, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/cpu.h"
#include "hal/list.h"
#include "hal/armv8m/mcx/n94x/mcxn94x.h"

#include "proc/userintr.h"

#include "config.h"

static struct {
	spinlock_t spinlock;
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts;


extern int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


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
		_hal_nvicSetPriority(h->n - 0x10, 1);
		_hal_nvicSetIRQ(h->n - 0x10, 1);
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
		_hal_nvicSetIRQ(h->n - 0x10, 0);
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

	hal_spinlockCreate(&interrupts.spinlock, "interrupts.spinlock");

	_hal_scbSetPriority(SYSTICK_IRQ, 1);
	_hal_scbSetPriority(PENDSV_IRQ, 1);
	_hal_scbSetPriority(SVC_IRQ, 0);

	/* Set no subprorities in Interrupt Group Priority */
	_hal_scbSetPriorityGrouping(3);

	return;
}
