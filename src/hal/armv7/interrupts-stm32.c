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


#define SIZE_INTERRUPTS		84
#define SIZE_HANDLERS		4


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
			if (h->pmap != NULL)
				userintr_dispatch(n, h);
			else
				h->f(n, ctx, h->data);
		} while ((h = h->next) != interrupts.handlers[n]);
	}

	hal_spinlockClear(&interrupts.spinlock);

	return;
}


void _hal_invokePendSV(void)
{
	_stm32_invokePendSV();
}


int hal_interruptsSetHandler(unsigned int n, intr_handler_t *h)
{
	if (n >= SIZE_INTERRUPTS || h == NULL || h->f == NULL)
		return -EINVAL;

	hal_spinlockSet(&interrupts.spinlock);
	_intr_add(&interrupts.handlers[n], h);
	hal_spinlockClear(&interrupts.spinlock);

	if (n >= 0x10) {
		_stm32_nvicSetIRQ(n - 0x10, 1);
		_stm32_nvicSetPriority(n, 0xf);
	}

	return EOK;
}


int hal_interruptsSetGpioInterrupt(unsigned char port, unsigned char pin, char state, char edge)
{
	_stm32_syscfgExtiLineConfig(port, pin);
	_stm32_extiMaskInterrupt(pin, state);
	_stm32_extiSetTrigger(pin, state, edge);

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
