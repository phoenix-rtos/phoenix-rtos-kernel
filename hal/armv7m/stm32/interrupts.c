/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling
 *
 * Copyright 2017, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/list.h"
#include "hal/cpu.h"

#include "proc/userintr.h"


#if defined(__CPU_STM32L152XD) || defined(__CPU_STM32L152XE)
#define SIZE_INTERRUPTS     84
#endif

#ifdef __CPU_STM32L4X6
#define SIZE_INTERRUPTS     97
#endif


struct {
	spinlock_t spinlock;
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts;


int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


void interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0;
	spinlock_ctx_t sc;

	if (n >= SIZE_INTERRUPTS)
		return;

	hal_spinlockSet(&interrupts.spinlock, &sc);

	interrupts.counters[n]++;

	h = interrupts.handlers[n];
	if (h != NULL) {
		do {
			hal_cpuSetGot(h->got);
			if (h->f(n, ctx, h->data)) {
				reschedule = 1;
			}
			h = h->next;
		} while (h != interrupts.handlers[n]);
	}

	hal_spinlockClear(&interrupts.spinlock, &sc);

	if (reschedule)
		threads_schedule(n, ctx, NULL);
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS)
		return -1;

	hal_spinlockSet(&interrupts.spinlock, &sc);
	h->got = hal_cpuGetGot();

	HAL_LIST_ADD(&interrupts.handlers[h->n], h);

	if (h->n >= 0x10) {
		_hal_scsIRQPrioritySet(h->n - 0x10, 1);
		_hal_scsIRQSet(h->n - 0x10, 1);
	}
	hal_spinlockClear(&interrupts.spinlock, &sc);

	return 0;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS)
		return -1;

	hal_spinlockSet(&interrupts.spinlock, &sc);
	HAL_LIST_REMOVE(&interrupts.handlers[h->n], h);

	if (h->n >= 0x10 && interrupts.handlers[h->n] == NULL)
		_hal_scsIRQSet(h->n - 0x10, 0);

	hal_spinlockClear(&interrupts.spinlock, &sc);

	return 0;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using NVIC interrupt controller", len);
	features[len - 1] = 0;

	return features;
}


__attribute__ ((section (".init"))) void _hal_interruptsInit(void)
{
	unsigned int n;

	for (n = 0; n < SIZE_INTERRUPTS; ++n) {
		interrupts.handlers[n] = NULL;
		interrupts.counters[n] = 0;
	}

	hal_spinlockCreate(&interrupts.spinlock, "interrupts.spinlock");

	_hal_scsExceptionPrioritySet(SYSTICK_IRQ, 1);
	_hal_scsExceptionPrioritySet(PENDSV_IRQ, 0);
	_hal_scsExceptionPrioritySet(SVC_IRQ, 0);

	/* Set no subprorities in Interrupt Group Priority */
	_hal_scsPriorityGroupingSet(3);

	return;
}
