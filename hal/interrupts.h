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

#ifndef _PH_HAL_INTERRUPTS_H_
#define _PH_HAL_INTERRUPTS_H_

#include <arch/interrupts.h>


int hal_interruptsSetHandler(intr_handler_t *h);


int hal_interruptsDeleteHandler(intr_handler_t *h);


char *hal_interruptsFeatures(char *features, size_t len);


void _hal_interruptsInit(void);


/* controls trace of non-systick interrupts */
void _hal_interruptsTrace(int enable);


#endif
