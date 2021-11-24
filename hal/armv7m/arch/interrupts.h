/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling
 *
 * Copyright 2016, 2017, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk, Artur Wodejko, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _HAL_ARMV7M_INTERRUPTS_H_
#define _HAL_ARMV7M_INTERRUPTS_H_

#include "cpu.h"

#define SVC_IRQ     11
#define PENDSV_IRQ  14
#define SYSTICK_IRQ 15

typedef struct _intr_handler_t {
	struct _intr_handler_t *next;
	struct _intr_handler_t *prev;
	unsigned int n;
	int (*f)(unsigned int, cpu_context_t *, void *);
	void *data;
	void *got;
} intr_handler_t;

#endif
