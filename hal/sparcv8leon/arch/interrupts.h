/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_LEON3_INTERRUPTS_H_
#define _PH_HAL_LEON3_INTERRUPTS_H_

#include "cpu.h"

typedef struct _intr_handler_t {
	struct _intr_handler_t *next;
	struct _intr_handler_t *prev;
	unsigned int n;
	intrFn_t f;
	void *data;
#ifdef NOMMU
	void *got;
#endif
} intr_handler_t;


#endif
