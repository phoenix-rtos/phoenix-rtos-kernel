/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception and interrupt handling
 *
 * Copyright 2016, 2012, 2020, 2024 Phoenix Systems
 * Copyright 2001, 2005, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_RISCV64_INTERRUPTS_H_
#define _HAL_RISCV64_INTERRUPTS_H_

#include "cpu.h"


#define TLB_IRQ     (1U | CLINT_IRQ_FLG)
#define SYSTICK_IRQ (5U | CLINT_IRQ_FLG)


typedef struct _intr_handler_t {
	struct _intr_handler_t *next;
	struct _intr_handler_t *prev;
	unsigned int n;
	int (*f)(unsigned int n, cpu_context_t *ctx, void *arg);
	void *data;
} intr_handler_t;


void hal_interruptsInitCore(void);


#endif
