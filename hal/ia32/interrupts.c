/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling
 *
 * Copyright 2012-2013, 2016-2017, 2020 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "halsyspage.h"
#include "../interrupts.h"
#include "../spinlock.h"
#include "../cpu.h"
#include "../pmap.h"
#include "ia32.h"

#include "../../proc/userintr.h"
#include "../../include/errno.h"


/* Hardware interrupt stubs */
extern void _interrupts_irq0(void);
extern void _interrupts_irq1(void);
extern void _interrupts_irq2(void);
extern void _interrupts_irq3(void);
extern void _interrupts_irq4(void);
extern void _interrupts_irq5(void);
extern void _interrupts_irq6(void);
extern void _interrupts_irq7(void);
extern void _interrupts_irq8(void);
extern void _interrupts_irq9(void);
extern void _interrupts_irq10(void);
extern void _interrupts_irq11(void);
extern void _interrupts_irq12(void);
extern void _interrupts_irq13(void);
extern void _interrupts_irq14(void);
extern void _interrupts_irq15(void);

extern void _interrupts_unexpected(void);

extern void _interrupts_syscall(void);


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


unsigned int _interrupts_multilock;


void _interrupts_apicACK(unsigned int n)
{
	if (n >= SIZE_INTERRUPTS)
		return;

if (hal_cpuGetID()) {
	__asm__ volatile
	("movl $0, (0xfee000b0)"::);
	return;
}

	if (n < 8) {
		hal_outb((void *)0x20, 0x60 | n);
	}
	else {
		hal_outb((void *)0x20, 0x62);
		hal_outb((void *)0xa0, 0x60 | (n - 8));
	}
	return;
}

int interrupts_dispatchIRQ(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0;
	spinlock_ctx_t sc;

	if (n >= SIZE_INTERRUPTS)
		return 0;

	hal_spinlockSet(&interrupts.spinlocks[n], &sc);

	interrupts.counters[n]++;

	if ((h = interrupts.handlers[n]) != NULL) {
		do
			if (h->f(n, ctx, h->data))
				reschedule = 1;
		while ((h = h->next) != interrupts.handlers[n]);
	}

	hal_spinlockClear(&interrupts.spinlocks[n], &sc);

	if (n == 0)
		return 0;
	return reschedule;
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


/* Function setups interrupt stub in IDT */
__attribute__ ((section (".init"))) int _interrupts_setIDTEntry(unsigned int n, void *addr, u32 type)
{
	u32 w0, w1;
	u32 *idtr;

	if (n > 255)
		return -EINVAL;

	w0 = ((u32)addr & 0xffff0000);
	w1 = ((u32)addr & 0x0000ffff);

	w0 |= IGBITS_DPL3 | IGBITS_PRES | IGBITS_SYSTEM | type;
	w1 |= (SEL_KCODE << 16);

	idtr = (u32 *)syspage->hs.idtr.addr;
	idtr[n * 2 + 1] = w0;
	idtr[n * 2] = w1;

	return EOK;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using i8259 interrupt controller", len);
	features[len - 1] = 0;

	return features;
}


__attribute__ ((section (".init"))) void _hal_interruptsInit(void)
{
	unsigned int k;

	_interrupts_multilock = 1;

	/* Initialize interrupt controllers (8259A) */
	hal_outb((void *)0x20, 0x11);  /* ICW1 */
	hal_outb((void *)0x21, 0x20);  /* ICW2 (Master) */
	hal_outb((void *)0x21, 0x04);  /* ICW3 (Master) */
	hal_outb((void *)0x21, 0x01);  /* ICW4 */

	hal_outb((void *)0xa0, 0x11);  /* ICW1 (Slave) */
	hal_outb((void *)0xa1, 0x28);  /* ICW2 (Slave) */
	hal_outb((void *)0xa1, 0x02);  /* ICW3 (Slave) */
	hal_outb((void *)0xa1, 0x01);  /* ICW4 (Slave) */

	/* Set stubs for hardware interrupts */
	_interrupts_setIDTEntry(32 + 0,  _interrupts_irq0, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 1,  _interrupts_irq1, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 2,  _interrupts_irq2, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 3,  _interrupts_irq3, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 4,  _interrupts_irq4, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 5,  _interrupts_irq5, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 6,  _interrupts_irq6, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 7,  _interrupts_irq7, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 8,  _interrupts_irq8, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 9,  _interrupts_irq9, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 10, _interrupts_irq10, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 11, _interrupts_irq11, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 12, _interrupts_irq12, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 13, _interrupts_irq13, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 14, _interrupts_irq14, IGBITS_IRQEXC);
	_interrupts_setIDTEntry(32 + 15, _interrupts_irq15, IGBITS_IRQEXC);

	for (k = 0; k < SIZE_INTERRUPTS; k++) {
		interrupts.handlers[k] = NULL;
		interrupts.counters[k] = 0;
		hal_spinlockCreate(&interrupts.spinlocks[k], "interrupts.spinlocks[]");
	}

	/* Set stubs for unhandled interrupts */
	for (; k < 256 - SIZE_INTERRUPTS; k++)
		_interrupts_setIDTEntry(32 + k, _interrupts_unexpected, IGBITS_IRQEXC);

	/* Set stub for syscall */
/*	_interrupts_setIDTEntry(0x80, _interrupts_syscall, IGBITS_TRAP); */
	_interrupts_setIDTEntry(0x80, _interrupts_syscall, IGBITS_IRQEXC);

	return;
}
