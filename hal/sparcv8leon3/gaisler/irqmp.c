/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling - IRQMP controller
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/cpu.h"
#include "hal/spinlock.h"
#include "hal/interrupts.h"
#include "proc/userintr.h"

#include "config.h"


extern unsigned int _end;


#ifdef NOMMU
#define VADDR_INT_CTRL INT_CTRL_BASE
#else
#define VADDR_INT_CTRL (void *)((u32)VADDR_PERIPH_BASE + PAGE_OFFS_INT_CTRL)
#endif

#define SIZE_INTERRUPTS 32
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


/* Interrupt controller */

#define INT_LEVEL   0  /* Interrupt level register : 0x00 */
#define INT_PEND    1  /* Interrupt pending register : 0x04 */
#define INT_FORCE   2  /* Interrupt force register (CPU 0) : 0x08 */
#define INT_CLEAR   3  /* Interrupt clear register : 0x0C */
#define INT_MPSTAT  4  /* Multiprocessor status register : 0x10 */
#define INT_BRDCAST 5  /* Broadcast register : 0x14 */
#define INT_MASK_0  16 /* Interrupt mask register (CPU 0) : 0x40 */
#define INT_MASK_1  17 /* Interrupt mask register (CPU 1) : 0x44 */
#define INT_FORCE_0 32 /* Interrupt force register (CPU 0) : 0x80 */
#define INT_FORCE_1 33 /* Interrupt force register (CPU 1) : 0x84 */
#define INT_EXTID_0 48 /* Extended interrupt ID register (CPU 0) : 0xC0 */
#define INT_EXTID_1 49 /* Extended interrupt ID register (CPU 1) : 0xC4 */


struct {
	volatile u32 *int_ctrl;
	spinlock_t spinlocks[SIZE_INTERRUPTS];
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts_common;


extern int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


void hal_cpuBroadcastIPI(unsigned int intr)
{
	unsigned int id = hal_cpuGetID(), i;

	for (i = 0; i < hal_cpuGetCount(); ++i) {
		if (i != id) {
			*(interrupts_common.int_ctrl + INT_FORCE_0 + i) |= (1 << intr);
		}
	}
}


void hal_cpuStartCores(void)
{
	unsigned int id = hal_cpuGetID(), i;
	u32 msk = 0;

	if (id == 0) {
		for (i = 1; i < NUM_CPUS; ++i) {
			msk |= (1 << i);
		}
		*(interrupts_common.int_ctrl + INT_MPSTAT) = msk;
	}
}


void interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0, cpuID = hal_cpuGetID();
	spinlock_ctx_t sc;

	if (n == EXTENDED_IRQN) {
		/* Extended interrupt (16 - 31) */
		n = *(interrupts_common.int_ctrl + INT_EXTID_0 + cpuID) & 0x3F;
	}

	if (n >= SIZE_INTERRUPTS) {
		return;
	}

	hal_spinlockSet(&interrupts_common.spinlocks[n], &sc);

	interrupts_common.counters[n]++;
	h = interrupts_common.handlers[n];
	if (h != NULL) {
		do {
#ifdef NOMMU
			hal_cpuSetGot(h->got);
#endif
			reschedule |= h->f(n, ctx, h->data);
			h = h->next;
		} while (h != interrupts_common.handlers[n]);
	}

	if (reschedule != 0) {
		threads_schedule(n, ctx, NULL);
	}

	hal_spinlockClear(&interrupts_common.spinlocks[n], &sc);
}


static void interrupts_enableIRQ(unsigned int irqn)
{
	int i;

	/* TLB and Systick IRQ should fire on all cores */
	if ((irqn == TLB_IRQ) || (irqn == TIMER_IRQ)) {
		for (i = 0; i < hal_cpuGetCount(); ++i) {
			*(interrupts_common.int_ctrl + INT_MASK_0 + i) |= (1 << irqn);
		}
	}
	else {
		/* Other IRQs only on core 0 - no easy way to manage them */
		*(interrupts_common.int_ctrl + INT_MASK_0) |= (1 << irqn);
	}
}


static void interrupts_disableIRQ(unsigned int irqn)
{
	int i;

	for (i = 0; i < hal_cpuGetCount(); ++i) {
		*(interrupts_common.int_ctrl + INT_MASK_0 + i) &= ~(1 << irqn);
	}
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->n >= SIZE_INTERRUPTS) {
		return -1;
	}

	hal_spinlockSet(&interrupts_common.spinlocks[h->n], &sc);
#ifdef NOMMU
	h->got = hal_cpuGetGot();
#endif
	_intr_add(&interrupts_common.handlers[h->n], h);
	interrupts_enableIRQ(h->n);
	hal_spinlockClear(&interrupts_common.spinlocks[h->n], &sc);

	return 0;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->n >= SIZE_INTERRUPTS) {
		return -1;
	}

	hal_spinlockSet(&interrupts_common.spinlocks[h->n], &sc);
	_intr_remove(&interrupts_common.handlers[h->n], h);

	if (interrupts_common.handlers[h->n] == NULL) {
		interrupts_disableIRQ(h->n);
	}

	hal_spinlockClear(&interrupts_common.spinlocks[h->n], &sc);

	return 0;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using IRQMP interrupt controller", len);
	features[len - 1] = 0;

	return features;
}

void _hal_interruptsInit(void)
{
	int i;

	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		hal_spinlockCreate(&interrupts_common.spinlocks[i], "interrupts_common");
		interrupts_common.handlers[i] = NULL;
		interrupts_common.counters[i] = 0;
	}

	interrupts_common.int_ctrl = (void *)VADDR_INT_CTRL;
	*(interrupts_common.int_ctrl + INT_CLEAR) = 0xffffffff;
}
