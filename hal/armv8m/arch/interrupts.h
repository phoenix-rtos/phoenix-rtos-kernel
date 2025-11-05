/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling
 *
 * Copyright 2016, 2017, 2020, 2022 Phoenix Systems
 * Author: Pawel Pisarczyk, Artur Wodejko, Hubert Buczynski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _PH_HAL_ARMV8M_INTERRUPTS_H_
#define _PH_HAL_ARMV8M_INTERRUPTS_H_

#include "cpu.h"
#include "hal/arm/scs.h"

#define SVC_IRQ     11
#define PENDSV_IRQ  14
#define SYSTICK_IRQ 15


typedef struct _intr_handler_t {
	struct _intr_handler_t *next;
	struct _intr_handler_t *prev;
	/* irq */
	unsigned int n;
	/* handler function */
	intrFn_t f;
	void *data;
	void *got;
} intr_handler_t;

#endif
