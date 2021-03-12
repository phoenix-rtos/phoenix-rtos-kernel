/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception and interrupt handling for Zynq-7000
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "cpu.h"
#include "pmap.h"
#include "spinlock.h"
#include "interrupts.h"

#include "../../proc/userintr.h"
#include "../../include/errno.h"

extern int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


#define SIZE_INTERRUPTS     0
#define SIZE_HANDLERS       4


struct {
	volatile u32 *gic;
	spinlock_t spinlock[SIZE_INTERRUPTS];
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts;


extern void _end(void);


void interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{

}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	return 0;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	return 0;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	return 0;
}


/* Function initializes interrupt handling */
void _hal_interruptsInit(void)
{

}
