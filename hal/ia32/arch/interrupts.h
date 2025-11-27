/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception and interrupt handling
 *
 * Copyright 2016, 2012, 2023 Phoenix Systems
 * Copyright 2001, 2005, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Andrzej Stalke
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_IA32_INTERRUPTS_H_
#define _PH_HAL_IA32_INTERRUPTS_H_

#include "cpu.h"

#define INTERRUPTS_VECTOR_OFFSET 32U

#define SYSTICK_IRQ 0U
#define SYSCALL_IRQ 0x80U
#define TLB_IRQ     0x81U

#define IOAPIC_IDREG  0x0U
#define IOAPIC_VERREG 0x1U
#define IOAPIC_ARBREG 0x2U

#define IOAPIC_IRQ_MASK (1UL << 16)
#define IOAPIC_TRIGGER  (1UL << 15)
#define IOAPIC_INTPOL   (1UL << 13)
#define IOAPIC_DESTMOD  (1UL << 11)

#define LAPIC_EOI 0U

/* Interrupt source override polarity flags */
#define MADT_ISO_POLAR_MASK 0x3U
#define MADT_ISO_POLAR_BUS  0x0U
#define MADT_ISO_POLAR_HIGH 0x1U
#define MADT_ISO_POLAR_LOW  0x3U
/* Interrupt source override trigger flags */
#define MADT_ISO_TRIGGER_MASK  (0x3U << 2)
#define MADT_ISO_TRIGGER_BUS   0x0U
#define MADT_ISO_TRIGGER_EDGE  (0x1U << 2)
#define MADT_ISO_TRIGGER_LEVEL (0x3U << 2)

#ifndef __ASSEMBLY__

typedef struct _intr_handler_t {
	struct _intr_handler_t *next;
	struct _intr_handler_t *prev;
	unsigned int n;
	intrFn_t f;
	void *data;
} intr_handler_t;

#endif


#endif
