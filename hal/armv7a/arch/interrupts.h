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

#ifndef _PH_HAL_ARMV7A_INTERRUPTS_H_
#define _PH_HAL_ARMV7A_INTERRUPTS_H_

#include "cpu.h"

typedef struct _intr_handler_t {
	struct _intr_handler_t *next;
	struct _intr_handler_t *prev;
	unsigned int n;
	intrFn_t f;
	void *data;
} intr_handler_t;


#endif
