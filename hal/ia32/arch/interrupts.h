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

#define INTERRUPTS_VECTOR_OFFSET 32

#define SYSTICK_IRQ 0
#define SYSCALL_IRQ 0x80
#define TLB_IRQ     0x81

#define IOAPIC_IDREG  0x0
#define IOAPIC_VERREG 0x1
#define IOAPIC_ARBREG 0x2

#define IOAPIC_IRQ_MASK (1u << 16)
#define IOAPIC_TRIGGER  (1u << 15)
#define IOAPIC_INTPOL   (1u << 13)
#define IOAPIC_DESTMOD  (1u << 11)

#define LAPIC_EOI 0

/* Interrupt source override polarity flags */
#define MADT_ISO_POLAR_MASK 0x3
#define MADT_ISO_POLAR_BUS  0x0
#define MADT_ISO_POLAR_HIGH 0x1
#define MADT_ISO_POLAR_LOW  0x3
/* Interrupt source override trigger flags */
#define MADT_ISO_TRIGGER_MASK  (0x3 << 2)
#define MADT_ISO_TRIGGER_BUS   0x0
#define MADT_ISO_TRIGGER_EDGE  (0x1u << 2)
#define MADT_ISO_TRIGGER_LEVEL (0x3u << 2)

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
