/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling (RISCV64)
 *
 * Copyright 2018, 2020 Phoenix Systems
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
#include "sbi.h"

#include "../../proc/userintr.h"

#include "../../include/errno.h"



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


void interrupts_dispatchIRQ(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	spinlock_ctx_t sc;
	unsigned int cn;

	if (n >= SIZE_INTERRUPTS)
		return;

if (n != 0) {
	cn = plic_claim(0);
	if (cn == 0) {
		return;
	}
}

	hal_spinlockSet(&interrupts.spinlocks[n], &sc);

	interrupts.counters[n]++;

	if ((h = interrupts.handlers[n]) != NULL) {
		do
			h->f(n, ctx, h->data);
		while ((h = h->next) != interrupts.handlers[n]);
	}

	hal_spinlockClear(&interrupts.spinlocks[n], &sc);

if (cn != 0) {
	plic_complete(0, cn);

//	lib_printf("cl=%p\n", n);
}


//for (;;);
	return;
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS)
		return -EINVAL;

	hal_spinlockSet(&interrupts.spinlocks[h->n], &sc);
	_intr_add(&interrupts.handlers[h->n], h);
	hal_spinlockClear(&interrupts.spinlocks[h->n], &sc);

	return EOK;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS)
		return -EINVAL;

	hal_spinlockSet(&interrupts.spinlocks[h->n], &sc);
	_intr_remove(&interrupts.handlers[h->n], h);
	hal_spinlockClear(&interrupts.spinlocks[h->n], &sc);

	return EOK;
}


__attribute__((aligned(4))) void handler(cpu_context_t *ctx)
{
	cycles_t c = hal_cpuGetCycles2();
	sbi_ecall(SBI_SETTIMER, 0, c + 10000, 0, 0, 0, 0, 0);
}


extern void interrupts_handleintexc(void *);


__attribute__ ((section (".init"))) void _hal_interruptsInit(void)
{
	unsigned int k;

	for (k = 0; k < SIZE_INTERRUPTS; k++) {
		interrupts.handlers[k] = NULL;
		interrupts.counters[k] = 0;
		hal_spinlockCreate(&interrupts.spinlocks[k], "interrupts.spinlocks[]");
	}

	/* Enable HART interrupts */
	csr_write(sscratch, 0);
	csr_write(sie, -1);

	csr_write(stvec, interrupts_handleintexc);

	/* Initialize PLIC if present */
	_plic_init();

	return;
}
