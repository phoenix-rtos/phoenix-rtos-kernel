/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception and interrupt handling
 *
 * Copyright 2016, 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_AARCH64_INTERRUPTS_H_
#define _HAL_AARCH64_INTERRUPTS_H_

#include "cpu.h"

typedef struct _intr_handler_t {
	struct _intr_handler_t *next;
	struct _intr_handler_t *prev;
	unsigned int n;
	intrFn_t f;
	void *data;
} intr_handler_t;


#endif
