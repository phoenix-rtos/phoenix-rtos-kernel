/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "interrupts.h"
#include "spinlock.h"
#include "cpu.h"
#include "pmap.h"
#include "imxrt.h"

#include "../../proc/userintr.h"

#include "../../../include/errno.h"


#define SIZE_INTERRUPTS		167


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
	spinlock_t spinlocks[SIZE_INTERRUPTS];
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts;


void interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;

	if (n >= SIZE_INTERRUPTS)
		return;

	hal_spinlockSet(&interrupts.spinlocks[n]);

	interrupts.counters[n]++;

	if ((h = interrupts.handlers[n]) != NULL) {
		do {
			if (h->pmap != NULL)
				userintr_dispatch(n, h);
			else
				h->f(n, ctx, h->data);
		} while ((h = h->next) != interrupts.handlers[n]);
	}

	hal_spinlockClear(&interrupts.spinlocks[n]);

	return;
}


void _hal_invokePendSV(void)
{
	_imxrt_invokePendSV();
}


int hal_interruptsSetHandler(unsigned int n, intr_handler_t *h)
{
	if (n >= SIZE_INTERRUPTS || h == NULL || h->f == NULL)
		return -EINVAL;

	hal_spinlockSet(&interrupts.spinlocks[n]);
	_intr_add(&interrupts.handlers[n], h);
	hal_spinlockClear(&interrupts.spinlocks[n]);

	if (n >= 0x10)
		_imxrt_nvicSetIRQ(n - 0x10, 1);

	return EOK;
}


int hal_interruptsSetGpioInterrupt(unsigned char port, unsigned char pin, char state, char edge)
{
	/* Not implemented */

	return EOK;
}


__attribute__ ((section (".init"))) void _hal_interruptsInit(void)
{
	unsigned int n;

	for (n = 0; n < SIZE_INTERRUPTS; ++n) {
		interrupts.handlers[n] = NULL;
		interrupts.counters[n] = 0;
		hal_spinlockCreate(&interrupts.spinlocks[n], "interrupts.spinlocks[]");
	}

	_imxrt_scbSetPriority(SYSTICK_IRQ, 15);
	_imxrt_scbSetPriority(SVC_IRQ, 11);
	_imxrt_scbSetPriority(PENDSV_IRQ, 14);

	/* Set no subprorities in Interrupt Group Priority */
	_imxrt_scbSetPriorityGrouping(3);

	return;
}
