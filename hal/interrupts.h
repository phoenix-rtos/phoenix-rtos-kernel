/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception and interrupt handling
 *
 * Copyright 2016, 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_INTERRUPTS_H_
#define _HAL_INTERRUPTS_H_

#include <arch/interrupts.h>


extern int hal_interruptsSetHandler(intr_handler_t *h);


extern int hal_interruptsDeleteHandler(intr_handler_t *h);


extern char *hal_interruptsFeatures(char *features, unsigned int len);


extern void _hal_interruptsInit(void);


/* controls trace of non-systick interrupts */
extern void _hal_interruptsTrace(int enable);


#endif
