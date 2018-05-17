/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception and interrupt handling
 *
 * Copyright 2016, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_INTERRUPTS_H_
#define _HAL_INTERRUPTS_H_

#ifndef __ASSEMBLY__

#include "cpu.h"
#include "pmap.h"


#define HPTIMER_IRQ     88


typedef struct _intr_handler_t {
	struct _intr_handler_t *next;
	struct _intr_handler_t *prev;
	unsigned int n;
	int (*f)(unsigned int, cpu_context_t *, void *);
	void *data;
	pmap_t *pmap;
	void *cond;
} intr_handler_t;


/* Function installs new handler for interrupt given by n */
extern int hal_interruptsSetHandler(intr_handler_t *h);


extern int hal_interruptsDeleteHandler(intr_handler_t *h);


/* Function initializes interrupt handling */
extern void _hal_interruptsInit(void);


#endif

#endif
