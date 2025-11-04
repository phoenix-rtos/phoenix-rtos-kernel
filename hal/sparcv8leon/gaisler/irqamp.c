/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling - IRQAMP controller
 *
 * Copyright 2022, 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/cpu.h"
#include "hal/list.h"
#include "hal/string.h"
#include "hal/spinlock.h"
#include "hal/interrupts.h"

#include <arch/pmap.h>

#include <config.h>


#define SIZE_INTERRUPTS 32

/* clang-format off */

/* Interrupt controller
 * NOTE: Some registers may not be available depending on the configuration
 */
enum {
	int_level = 0,     /* Interrupt level register                   : 0x000 */
	int_pend,          /* Interrupt pending register                 : 0x004 */
	int_force,         /* Interrupt force register (CPU 0)           : 0x008 */
	int_clear,         /* Interrupt clear register                   : 0x00C */
	int_mpstat,        /* Status register                            : 0x010 */
	broadcast,         /* Broadcast register                         : 0x014 */
	errstat,           /* Error mode status register                 : 0x018 */
	wdogctrl,          /* Watchdog control register                  : 0x01C */
	asmpctrl,          /* Asymmetric MP control register             : 0x020 */
	icselr,            /* Interrupt controller select register       : 0x024 */
	                   /* Reserved                                   : 0x028 - 0x030 */
	eint_clear = 13,   /* Extended interrupt clear register          : 0x034 */
	                   /* Reserved                                   : 0x038 - 0x03C */
	pi_mask = 16,      /* Processor interrupt mask register (CPU 0)  : 0x040 */
	                   /* NUM_CPUS pi_mask registers                         */
	pc_force = 32,     /* Processor interrupt force register (CPU 0) : 0x080 */
	                   /* NUM_CPUS pc_force registers                        */
	pextack = 48,      /* Extended int acknowledge register (CPU 0)  : 0x0C0 */
	                   /* NUM_CPUS pextack registers                         */
	tcnt0 = 64,        /* Interrupt timestamp 0 counter register     : 0x100 */
	istmpc0,           /* Timestamp 0 control register               : 0x104 */
	itstmpas0,         /* Interrupt assertion timestamp 0 register   : 0x108 */
	itstmpack0,        /* Interrupt acknowledge timestamp 0 register : 0x10C */
	tcnt1,             /* Interrupt timestamp 1 counter register     : 0x110 */
	istmpc1,           /* Timestamp 1 control register               : 0x114 */
	itstmpas1,         /* Interrupt assertion timestamp 1 register   : 0x118 */
	itstmpack1,        /* Interrupt acknowledge timestamp 1 register : 0x11C */
	tcnt2,             /* Interrupt timestamp 2 counter register     : 0x120 */
	istmpc2,           /* Timestamp 2 control register               : 0x124 */
	itstmpas2,         /* Interrupt assertion timestamp 2 register   : 0x128 */
	itstmpack2,        /* Interrupt acknowledge timestamp 2 register : 0x12C */
	tcnt3,             /* Interrupt timestamp 3 counter register     : 0x130 */
	istmpc3,           /* Timestamp 3 control register               : 0x134 */
	itstmpas3,         /* Interrupt assertion timestamp 3 register   : 0x138 */
	itstmpack3,        /* Interrupt acknowledge timestamp 3 register : 0x13C */
	                   /* Reserved                                   : 0x140 - 0x1FC */
	procbootadr = 128, /* Processor boot address register (CPU 0)    : 0x200 */
	                   /* NUM_CPUS procbootadr registers                     */
	irqmap = 192,      /* Interrupt map register                     : 0x300 - impl dependent no. entries */
};

/* clang-format on */


static struct {
	volatile u32 *int_ctrl;
	u32 extendedIrqn;
	spinlock_t spinlocks[SIZE_INTERRUPTS];
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts_common;


int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


void hal_cpuBroadcastIPI(unsigned int intr)
{
	unsigned int id = hal_cpuGetID(), i;

	for (i = 0; i < hal_cpuGetCount(); ++i) {
		if (i != id) {
			*(interrupts_common.int_ctrl + pc_force + i) |= (1 << intr);
		}
	}
}


void hal_cpuStartCores(void)
{
	unsigned int id = hal_cpuGetID();
	u32 msk = 0;

	if (id == 0) {
		msk = ((1 << hal_cpuGetCount()) - 1) & ~(1 << id);
		*(interrupts_common.int_ctrl + int_mpstat) = msk;
	}
}


void interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0, cpuid = hal_cpuGetID();
	spinlock_ctx_t sc;

	if (n == interrupts_common.extendedIrqn) {
		/* Extended interrupt (16 - 31) */
		n = *(interrupts_common.int_ctrl + pextack + cpuid) & 0x3F;
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

	/* TLB and Wakeup Timer IRQ should fire on all cores */
	if ((irqn == TLB_IRQ) || (irqn == TIMER0_2_IRQ)) {
		for (i = 0; i < hal_cpuGetCount(); ++i) {
			*(interrupts_common.int_ctrl + pi_mask + i) |= (1 << irqn);
		}
		*(interrupts_common.int_ctrl + broadcast) |= (1 << irqn);
	}
	else {
		/* Other IRQs only on core 0 - no easy way to manage them */
		*(interrupts_common.int_ctrl + pi_mask) |= (1 << irqn);
	}
}


static void interrupts_disableIRQ(unsigned int irqn)
{
	int i;

	for (i = 0; i < hal_cpuGetCount(); i++) {
		*(interrupts_common.int_ctrl + pi_mask + i) &= ~(1 << irqn);
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
	HAL_LIST_ADD(&interrupts_common.handlers[h->n], h);
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
	HAL_LIST_REMOVE(&interrupts_common.handlers[h->n], h);

	if (interrupts_common.handlers[h->n] == NULL) {
		interrupts_disableIRQ(h->n);
	}

	hal_spinlockClear(&interrupts_common.spinlocks[h->n], &sc);

	return 0;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using IRQAMP interrupt controller", len);
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

	interrupts_common.int_ctrl = _pmap_halMapDevice(PAGE_ALIGN(INT_CTRL_BASE), PAGE_OFFS(INT_CTRL_BASE), SIZE_PAGE);

	/* Read extended irqn */
	interrupts_common.extendedIrqn = (*(interrupts_common.int_ctrl + int_mpstat) >> 16) & 0xf;
}
