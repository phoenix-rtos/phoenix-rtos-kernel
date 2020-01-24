/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "interrupts.h"
#include "spinlock.h"
#include "cpu.h"
#include "pmap.h"
#include "stm32.h"

#include "../../proc/userintr.h"

#include "../../../include/errno.h"

#ifdef CPU_STM32L1
#define SIZE_INTERRUPTS     84
#else
#define SIZE_INTERRUPTS     97
#endif
#define SIZE_HANDLERS       4


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


struct {
	spinlock_t spinlock;
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts;


void interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;

	if (n >= SIZE_INTERRUPTS)
		return;

	/* Due to no SMP in Cortex-M3 processor line, spinlock per irq has been removed to save space */
	hal_spinlockSet(&interrupts.spinlock);

	interrupts.counters[n]++;

	if ((h = interrupts.handlers[n]) != NULL) {
		do {
			hal_cpuSetGot(h->got);
			h->f(n, ctx, h->data);
		} while ((h = h->next) != interrupts.handlers[n]);
	}

	hal_spinlockClear(&interrupts.spinlock);

	return;
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS)
		return -EINVAL;

	hal_spinlockSet(&interrupts.spinlock);
	h->got = hal_cpuGetGot();

	_intr_add(&interrupts.handlers[h->n], h);

	if (h->n >= 0x10) {
		_stm32_nvicSetIRQ(h->n - 0x10, 1);
		_stm32_nvicSetPriority(h->n - 0x10, 0xf);
	}
	hal_spinlockClear(&interrupts.spinlock);

	return EOK;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS)
		return -EINVAL;

	hal_spinlockSet(&interrupts.spinlock);
	_intr_remove(&interrupts.handlers[h->n], h);

	if (h->n >= 0x10 && interrupts.handlers[h->n] == NULL)
		_stm32_nvicSetIRQ(h->n - 0x10, 0);

	hal_spinlockClear(&interrupts.spinlock);

	return EOK;
}


__attribute__ ((section (".init"))) void _hal_interruptsInit(void)
{
	unsigned int n;

	for (n = 0; n < SIZE_INTERRUPTS; ++n) {
		interrupts.handlers[n] = NULL;
		interrupts.counters[n] = 0;
	}

	hal_spinlockCreate(&interrupts.spinlock, "interrupts.spinlock");

	_stm32_scbSetPriority(SYSTICK_IRQ, 2);
	_stm32_scbSetPriority(PENDSV_IRQ, 1);
	_stm32_scbSetPriority(SVC_IRQ, 0);

	/* Set no subprorities in Interrupt Group Priority */
	_stm32_scbSetPriorityGrouping(3);

	return;
}
