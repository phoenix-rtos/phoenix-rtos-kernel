/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling
 *
 * Copyright 2016, 2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Artur Wodejko
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


#define SVC_IRQ			11
#define PENDSV_IRQ		14
#define SYSTICK_IRQ		15

#ifdef CPU_STM32
#define EXTI0_IRQ		22
#define EXTI1_IRQ		23
#define EXTI2_IRQ		24
#define EXTI3_IRQ		25
#define EXTI4_IRQ		26
#define EXTI9_5_IRQ		39
#define EXTI15_10_IRQ	56
#endif


typedef struct _intr_handler_t {
	struct _intr_handler_t *next;
	struct _intr_handler_t *prev;
	unsigned int n;
	int (*f)(unsigned int, cpu_context_t *, void *);
	void *data;
	struct _process_t *pmap;
	void *cond;
	void *got;
} intr_handler_t;


/* Function invokes PendSV exception in software */
extern void _hal_invokePendSV(void);


/* Function installs new handler for interrupt given by n */
extern int hal_interruptsSetHandler(intr_handler_t *h);


extern int hal_interruptsDeleteHandler(intr_handler_t *h);


extern int hal_interruptsSetGpioInterrupt(unsigned char port, unsigned char pin, char state, char edge);


/* Function initializes interrupt handling */
extern void _hal_interruptsInit(void);


#endif

#endif
