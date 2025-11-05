/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling for TI VIM (Vectored Interrupt Manager)
 *
 * Copyright 2021, 2025 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/armv7r/armv7r.h"

#include "hal/cpu.h"
#include "hal/spinlock.h"
#include "hal/interrupts.h"
#include "hal/list.h"

#include "proc/userintr.h"

#define VIM_BASE_ADDRESS 0x40F80000
#define SIZE_INTERRUPTS  384

#define DEFAULT_PRIORITY 7


enum {
	vim_pid = 0,                     /* Revision register */
	vim_info,                        /* Info register */
	vim_priirq,                      /* Prioritized IRQ register */
	vim_prifiq,                      /* Prioritized FIQ register */
	vim_irqgsts,                     /* IRQ group status register */
	vim_fiqgsts,                     /* FIQ group status register */
	vim_irqvec,                      /* IRQ vector address register */
	vim_fiqvec,                      /* FIQ vector address register */
	vim_actirq,                      /* Active IRQ register */
	vim_actfiq,                      /* Active FIQ register */
	vim_dedvec = 12,                 /* DED vector address register */
	vim_raw_m = (0x400 / 4),         /* Raw status/set register */
	vim_sts_m = (0x404 / 4),         /* Interrupt enable status/clear register */
	vim_intr_en_set_m = (0x408 / 4), /* Interrupt enable set register */
	vim_intr_en_clr_m = (0x40c / 4), /* Interrupt enabled clear register */
	vim_irqsts_m = (0x410 / 4),      /* IRQ interrupt enable status/clear register */
	vim_fiqsts_m = (0x414 / 4),      /* FIQ interrupt enable status/clear register */
	vim_intmap_m = (0x418 / 4),      /* Interrupt map register */
	vim_inttype_m = (0x41c / 4),     /* Interrupt type register */
	vim_pri_int_n = (0x1000 / 4),    /* Interrupt priority register */
	vim_vec_int_n = (0x2000 / 4),    /* Interrupt vector register */
};


static struct {
	volatile u32 *vim;
	spinlock_t spinlock[SIZE_INTERRUPTS];
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts_common;


int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


static void interrupts_clearStatus(unsigned int irqn)
{
	unsigned int irq_reg = (irqn / 32) * 8;
	unsigned int irq_offs = irqn % 32;

	if (irqn >= SIZE_INTERRUPTS) {
		return;
	}

	*(interrupts_common.vim + vim_irqsts_m + irq_reg) = 1u << irq_offs;
}


static void interrupts_setPriority(unsigned int irqn, u32 priority)
{
	if (irqn >= SIZE_INTERRUPTS) {
		return;
	}

	*(interrupts_common.vim + vim_pri_int_n + irqn) = priority & 0xf;
}


static inline u32 interrupts_getPriority(unsigned int irqn)
{
	if (irqn >= SIZE_INTERRUPTS) {
		return 0;
	}

	return *(interrupts_common.vim + vim_pri_int_n + irqn) & 0xf;
}


int interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0;
	spinlock_ctx_t sc;
	u32 dummy, irq_val;

	/* This register is supposed to be used for ISR vector (pointer to code),
	 * but because lowest 2 bits are hardwired to 0, it cannot store Thumb code pointers.
	 * For this reason we only do dummy read from it and get ISR pointer from the table. */
	dummy = *(interrupts_common.vim + vim_irqvec);
	(void)dummy;

	irq_val = *(interrupts_common.vim + vim_actirq);
	if ((irq_val & (1 << 31)) == 0) {
		/* No interrupt is pending */
		return 0;
	}

	n = irq_val & 0x3ff;
	if (n >= SIZE_INTERRUPTS) {
		/* This shouldn't happen, but behave in a sane way if it does */
		*(interrupts_common.vim + vim_irqvec) = 0; /* Write any value */
		return 0;
	}

	hal_spinlockSet(&interrupts_common.spinlock[n], &sc);

	interrupts_common.counters[n]++;

	h = interrupts_common.handlers[n];
	if (h != NULL) {
		do {
			hal_cpuSetGot(h->got);
			reschedule |= h->f(n, ctx, h->data);
			h = h->next;
		} while (h != interrupts_common.handlers[n]);
	}

	if (reschedule) {
		threads_schedule(n, ctx, NULL);
	}

	interrupts_clearStatus(n);
	*(interrupts_common.vim + vim_irqvec) = 0; /* Write any value */

	hal_spinlockClear(&interrupts_common.spinlock[n], &sc);

	return reschedule;
}


static void interrupts_enableIRQ(unsigned int irqn)
{
	unsigned int irq_reg = (irqn / 32) * 8;
	unsigned int irq_offs = irqn % 32;

	if (irqn >= SIZE_INTERRUPTS) {
		return;
	}

	*(interrupts_common.vim + vim_intr_en_set_m + irq_reg) = 1u << irq_offs;
	hal_cpuDataMemoryBarrier();
}


static void interrupts_disableIRQ(unsigned int irqn)
{
	unsigned int irq_reg = (irqn / 32) * 8;
	unsigned int irq_offs = irqn % 32;

	if (irqn >= SIZE_INTERRUPTS) {
		return;
	}

	*(interrupts_common.vim + vim_intr_en_clr_m + irq_reg) = 1u << irq_offs;
	hal_cpuDataMemoryBarrier();
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if ((h == NULL) || (h->f == NULL) || (h->n >= SIZE_INTERRUPTS)) {
		return -1;
	}

	hal_spinlockSet(&interrupts_common.spinlock[h->n], &sc);
	h->got = hal_cpuGetGot();
	HAL_LIST_ADD(&interrupts_common.handlers[h->n], h);

	interrupts_setPriority(h->n, DEFAULT_PRIORITY);
	interrupts_enableIRQ(h->n);

	hal_spinlockClear(&interrupts_common.spinlock[h->n], &sc);

	return 0;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using VIM interrupt controller", len);
	features[len - 1] = 0;

	return features;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if ((h == NULL) || (h->f == NULL) || (h->n >= SIZE_INTERRUPTS)) {
		return -1;
	}

	hal_spinlockSet(&interrupts_common.spinlock[h->n], &sc);
	HAL_LIST_REMOVE(&interrupts_common.handlers[h->n], h);

	if (interrupts_common.handlers[h->n] == NULL) {
		interrupts_disableIRQ(h->n);
	}

	hal_spinlockClear(&interrupts_common.spinlock[h->n], &sc);

	return 0;
}


void hal_cpuBroadcastIPI(unsigned int intr)
{
	unsigned int irq_reg = (intr / 32) * 8;
	unsigned int irq_offs = intr % 32;

	if (intr >= SIZE_INTERRUPTS) {
		return;
	}

	*(interrupts_common.vim + vim_raw_m + irq_reg) = 1u << irq_offs;
	hal_cpuDataMemoryBarrier();
}


/* Function initializes interrupt handling */
void _hal_interruptsInit(void)
{
	unsigned int i;

	interrupts_common.vim = (void *)VIM_BASE_ADDRESS;

	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		interrupts_common.handlers[i] = NULL;
		interrupts_common.counters[i] = 0;
		hal_spinlockCreate(&interrupts_common.spinlock[i], "interrupts");
	}

	/* Clear pending and disable interrupts, set them to be handled by IRQ not FIQ */
	for (i = 0; i < (SIZE_INTERRUPTS + 31) / 32; i++) {
		*(interrupts_common.vim + vim_irqsts_m + (i * 8)) = 0xffffffff;
		*(interrupts_common.vim + vim_intr_en_clr_m + (i * 8)) = 0xffffffff;
		*(interrupts_common.vim + vim_intmap_m + (i * 8)) = 0;
	}

	/* Read then write any value to set any pending interrupt as handled */
	i = *(interrupts_common.vim + vim_irqvec);
	*(interrupts_common.vim + vim_irqvec) = 0;

	/* Set default priority */
	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		interrupts_setPriority(i, DEFAULT_PRIORITY);
	}
}
