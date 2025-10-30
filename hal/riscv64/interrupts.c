/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling (RISCV64)
 *
 * Copyright 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/cpu.h"
#include "hal/list.h"
#include "hal/string.h"
#include "plic.h"
#include "dtb.h"
#include "riscv64.h"

#include "include/errno.h"

#include <board_config.h>


#define CLINT_IRQ_SIZE 16U

#define EXT_IRQ 9U

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


static int interrupts_dispatchPlic(cpu_context_t *ctx)
{
	unsigned int reschedule = 0;
	intr_handler_t *h;
	spinlock_ctx_t sc;

	unsigned int irq = plic_claim(PLIC_SCONTEXT(hal_cpuGetID()));
	RISCV_FENCE(o, i);

	if (irq == 0U) {
		return 0;
	}

	hal_spinlockSet(&interrupts_common.plic.spinlocks[irq], &sc);

	interrupts_common.plic.counters[irq]++;

	h = interrupts_common.plic.handlers[irq];
	if (h != NULL) {
		do {
			reschedule |= (unsigned int)h->f(irq, NULL, h->data);
			h = h->next;
		} while (h != interrupts_common.plic.handlers[irq]);
	}

	if (reschedule != 0U) {
		(void)threads_schedule(irq, ctx, NULL);
	}

	hal_spinlockClear(&interrupts_common.plic.spinlocks[irq], &sc);

	plic_complete(PLIC_SCONTEXT(hal_cpuGetID()), irq);

	return (int)reschedule;
}


static int interrupts_dispatchClint(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	unsigned int reschedule = 0;
	spinlock_ctx_t sc;

	hal_spinlockSet(&interrupts_common.clint.spinlocks[n], &sc);

	interrupts_common.clint.counters[n]++;

	h = interrupts_common.clint.handlers[n];
	if (h != NULL) {
		do {
			reschedule |= (unsigned int)h->f(n, NULL, h->data);
			h = h->next;
		} while (h != interrupts_common.clint.handlers[n]);
	}

	if (reschedule != 0U) {
		(void)threads_schedule(n, ctx, NULL);
	}

	hal_spinlockClear(&interrupts_common.clint.spinlocks[n], &sc);

	return (int)reschedule;
}


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Definition in assembly" */
int interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	if ((n == EXT_IRQ) && (dtb_getPLIC() != 0)) {
		return interrupts_dispatchPlic(ctx);
	}

	return interrupts_dispatchClint(n, ctx);
}


static int interrupts_setPlic(intr_handler_t *h, enum irq_state enable)
{
	spinlock_ctx_t sc;

	if (h->n >= (unsigned int)PLIC_IRQ_SIZE) {
		return -EINVAL;
	}

	hal_spinlockSet(&interrupts_common.plic.spinlocks[h->n], &sc);

	if (enable == irq_enable) {
		HAL_LIST_ADD(&interrupts_common.plic.handlers[h->n], h);
		plic_priority(h->n, 2);
		(void)plic_enableInterrupt(PLIC_SCONTEXT(hal_cpuGetID()), h->n);
	}
	else {
		(void)plic_disableInterrupt(PLIC_SCONTEXT(hal_cpuGetID()), h->n);
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
		csr_set(sie, 1UL << h->n);
	}
	else {
		csr_clear(sie, 1UL << h->n);
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

	if ((h->n & CLINT_IRQ_FLG) != 0U) {
		h->n = h->n & (unsigned int)~CLINT_IRQ_FLG;
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

	if ((h->n & CLINT_IRQ_FLG) != 0U) {
		h->n = h->n & (unsigned int)~CLINT_IRQ_FLG;
		ret = interrupts_setClint(h, irq_disable);
	}
	else {
		ret = interrupts_setPlic(h, irq_disable);
	}

	return ret;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	if (dtb_getPLIC() != 0) {
		(void)hal_strncpy(features, "Using PLIC interrupt controller", len);
	}
	else {
		(void)hal_strncpy(features, "PLIC interrupt controller not found", len);
	}

	if (len != 0U) {
		features[len - 1U] = '\0';
	}

	return features;
}


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Definition in assembly" */
extern void _interrupts_dispatch(void);


void hal_interruptsInitCore(void)
{
	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "Need to assign function address to processor register" */
	csr_write(stvec, _interrupts_dispatch);

	if (dtb_getPLIC() != 0) {
		plic_initCore();
	}
}


__attribute__((section(".init"))) void _hal_interruptsInit(void)
{
	unsigned int i;

	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "Need to assign function address to processor register" */
	csr_write(stvec, _interrupts_dispatch);
	for (i = 0; i < CLINT_IRQ_SIZE; i++) {
		interrupts_common.clint.handlers[i] = NULL;
		interrupts_common.clint.counters[i] = 0;
		hal_spinlockCreate(&interrupts_common.clint.spinlocks[i], "interrupts_common.clint");
	}

	for (i = 0; i < (unsigned int)PLIC_IRQ_SIZE; i++) {
		interrupts_common.plic.handlers[i] = NULL;
		interrupts_common.plic.counters[i] = 0;
		hal_spinlockCreate(&interrupts_common.plic.spinlocks[i], "interrupts_common.plic");
	}

	/* Initialize PLIC if present */
	if (dtb_getPLIC() != 0) {
		plic_init();
	}
	csr_write(sie, -1);
}
