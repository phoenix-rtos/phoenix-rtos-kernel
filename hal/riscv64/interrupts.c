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

#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/cpu.h"
#include "hal/list.h"
#include "sbi.h"
#include "plic.h"
#include "dtb.h"
#include "riscv64.h"

#include "proc/userintr.h"
#include "include/errno.h"

#include <board_config.h>


#define CLINT_IRQ_SIZE 16

#define EXT_IRQ 9

/* clang-format off */
enum irq_state { irq_enable = 0, irq_disable };
/* clang-format on */


static struct {
	struct {
		spinlock_t spinlocks[CLINT_IRQ_SIZE];
		unsigned int counters[CLINT_IRQ_SIZE];
		intr_handler_t *handlers[CLINT_IRQ_SIZE];
	} clint;

	struct {
		spinlock_t spinlocks[PLIC_IRQ_SIZE];
		unsigned int counters[PLIC_IRQ_SIZE];
		intr_handler_t *handlers[PLIC_IRQ_SIZE];
	} plic;
} interrupts_common;


extern int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


static void interrupts_dispatchPlic(cpu_context_t *ctx)
{
	int reschedule = 0;
	intr_handler_t *h;
	spinlock_ctx_t sc;

	unsigned int irq = plic_claim(1);

	if (irq == 0) {
		return;
	}

	hal_spinlockSet(&interrupts_common.plic.spinlocks[irq], &sc);

	interrupts_common.plic.counters[irq]++;

	h = interrupts_common.plic.handlers[irq];
	if (h != NULL) {
		do {
			reschedule |= h->f(irq, NULL, h->data);
			h = h->next;
		} while (h != interrupts_common.plic.handlers[irq]);
	}

	if (reschedule != 0) {
		threads_schedule(irq, ctx, NULL);
	}

	hal_spinlockClear(&interrupts_common.plic.spinlocks[irq], &sc);

	plic_complete(1, irq);
}


static void interrupts_dispatchClint(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0;
	spinlock_ctx_t sc;

	hal_spinlockSet(&interrupts_common.clint.spinlocks[n], &sc);

	interrupts_common.clint.counters[n]++;

	h = interrupts_common.clint.handlers[n];
	if (h != NULL) {
		do {
			reschedule |= h->f(n, NULL, h->data);
			h = h->next;
		} while (h != interrupts_common.clint.handlers[n]);
	}

	if (reschedule != 0) {
		threads_schedule(n, ctx, NULL);
	}

	hal_spinlockClear(&interrupts_common.clint.spinlocks[n], &sc);
}


void interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	if ((n == EXT_IRQ) && (dtb_getPLIC() != 0)) {
		interrupts_dispatchPlic(ctx);
	}
	else {
		interrupts_dispatchClint(n, ctx);
	}
}


static int interrupts_setPlic(intr_handler_t *h, enum irq_state enable)
{
	spinlock_ctx_t sc;

	if (h->n >= PLIC_IRQ_SIZE) {
		return -EINVAL;
	}

	hal_spinlockSet(&interrupts_common.plic.spinlocks[h->n], &sc);

	if (enable == irq_enable) {
		HAL_LIST_ADD(&interrupts_common.plic.handlers[h->n], h);
		plic_priority(h->n, 2);
		plic_enableInterrupt(1, h->n);
	}
	else {
		plic_disableInterrupt(1, h->n);
		HAL_LIST_REMOVE(&interrupts_common.plic.handlers[h->n], h);
	}

	hal_spinlockClear(&interrupts_common.plic.spinlocks[h->n], &sc);

	return 0;
}


static int interrupts_setClint(intr_handler_t *h, enum irq_state enable)
{
	spinlock_ctx_t sc;

	if (h->n >= CLINT_IRQ_SIZE) {
		return -EINVAL;
	}

	hal_spinlockSet(&interrupts_common.clint.spinlocks[h->n], &sc);

	if (enable == irq_enable) {
		HAL_LIST_ADD(&interrupts_common.clint.handlers[h->n], h);
		csr_set(sie, 1u << h->n);
	}
	else {
		csr_clear(sie, 1u << h->n);
		HAL_LIST_REMOVE(&interrupts_common.clint.handlers[h->n], h);
	}


	hal_spinlockClear(&interrupts_common.clint.spinlocks[h->n], &sc);

	return 0;
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	int ret;

	if (h == NULL) {
		return -EINVAL;
	}

	if ((h->n & CLINT_IRQ_FLG) != 0) {
		h->n = h->n & ~CLINT_IRQ_FLG;
		ret = interrupts_setClint(h, irq_enable);
	}
	else {
		ret = interrupts_setPlic(h, irq_enable);
	}

	return ret;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	int ret;

	if (h == NULL) {
		return -EINVAL;
	}

	if ((h->n & CLINT_IRQ_FLG) != 0) {
		h->n = h->n & ~CLINT_IRQ_FLG;
		ret = interrupts_setClint(h, irq_disable);
	}
	else {
		ret = interrupts_setPlic(h, irq_disable);
	}

	return ret;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	if (dtb_getPLIC()) {
		hal_strncpy(features, "Using PLIC interrupt controller", len);
	}
	else {
		hal_strncpy(features, "PLIC interrupt controller not found", len);
	}

	features[len - 1] = '\0';

	return features;
}


extern void _interrupts_dispatch(void *);


__attribute__((section(".init"))) void _hal_interruptsInit(void)
{
	unsigned int i;

	for (i = 0; i < CLINT_IRQ_SIZE; i++) {
		interrupts_common.clint.handlers[i] = NULL;
		interrupts_common.clint.counters[i] = 0;
		hal_spinlockCreate(&interrupts_common.clint.spinlocks[i], "interrupts_common.clint");
	}

	for (i = 0; i < PLIC_IRQ_SIZE; i++) {
		interrupts_common.plic.handlers[i] = NULL;
		interrupts_common.plic.counters[i] = 0;
		hal_spinlockCreate(&interrupts_common.plic.spinlocks[i], "interrupts_common.plic");
	}

	/* Enable HART interrupts */
	csr_write(sscratch, 0);
	csr_write(sie, -1);
	csr_write(stvec, _interrupts_dispatch);

	/* Initialize PLIC if present */
	if (dtb_getPLIC()) {
		_plic_init();
	}
}
