/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling (RISCV64)
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "interrupts.h"
#include "spinlock.h"
#include "syspage.h"
#include "cpu.h"
#include "pmap.h"

#include "../../proc/userintr.h"

#include "../../../include/errno.h"



#define SIZE_INTERRUPTS 16


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


struct {
	spinlock_t spinlocks[SIZE_INTERRUPTS];
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts;


void interrupts_dispatchIRQ(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;

	if (n >= SIZE_INTERRUPTS)
		return;

	hal_spinlockSet(&interrupts.spinlocks[n]);

	interrupts.counters[n]++;

	if ((h = interrupts.handlers[n]) != NULL) {
		do {
			if (h->pmap != NULL) {
				userintr_dispatch(n, h);
			}
			else
				h->f(n, ctx, h->data);
		} while ((h = h->next) != interrupts.handlers[n]);
	}

//	_interrupts_apicACK(n);
	hal_spinlockClear(&interrupts.spinlocks[n]);

	return;
}


int hal_interruptsSetHandler(unsigned int n, intr_handler_t *h)
{
	if (n >= SIZE_INTERRUPTS || h == NULL || h->f == NULL)
		return -EINVAL;

	hal_spinlockSet(&interrupts.spinlocks[n]);
	_intr_add(&interrupts.handlers[n], h);
	hal_spinlockClear(&interrupts.spinlocks[n]);

	return EOK;
}


__attribute__ ((section (".init"))) void _hal_interruptsInit(void)
{
	unsigned int k;

	for (k = 0; k < SIZE_INTERRUPTS; k++) {
		interrupts.handlers[k] = NULL;
		interrupts.counters[k] = 0;
		hal_spinlockCreate(&interrupts.spinlocks[k], "interrupts.spinlocks[]");
	}
	
	return;
}
