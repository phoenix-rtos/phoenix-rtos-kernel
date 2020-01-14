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

#ifdef CPU_IMXRT1170
#include "imxrt1170.h"
#define SIZE_INTERRUPTS 217
#else
#include "imxrt.h"
#define SIZE_INTERRUPTS 167
#endif

#include "../../proc/userintr.h"

#include "../../../include/errno.h"


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
		_imxrt_nvicSetIRQ(h->n - 0x10, 1);
		_imxrt_nvicSetPriority(h->n - 0x10, 0xf);
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
		_imxrt_nvicSetIRQ(h->n - 0x10, 0);

	hal_spinlockClear(&interrupts.spinlock);

	return EOK;
}


__attribute__ ((section (".init"))) void _hal_interruptsInit(void)
{
	unsigned int n;

	for (n = 0; n < 16; ++n) {
		interrupts.handlers[n] = NULL;
		interrupts.counters[n] = 0;
	}

	for (; n < SIZE_INTERRUPTS; ++n) {
		interrupts.handlers[n] = NULL;
		interrupts.counters[n] = 0;
		_imxrt_nvicSetIRQ(n - 0x10, 0);
	}

	hal_spinlockCreate(&interrupts.spinlock, "interrupts.spinlock");

	_imxrt_scbSetPriority(SYSTICK_IRQ, 15);
	_imxrt_scbSetPriority(SVC_IRQ, 11);
	_imxrt_scbSetPriority(PENDSV_IRQ, 14);

	/* Set no subprorities in Interrupt Group Priority */
	_imxrt_scbSetPriorityGrouping(3);

	return;
}
