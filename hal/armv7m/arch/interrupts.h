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


#ifndef _PH_HAL_ARMV7M_INTERRUPTS_H_
#define _PH_HAL_ARMV7M_INTERRUPTS_H_

#include "cpu.h"
#include "hal/arm/scs.h"

#define SVC_IRQ     11U
#define PENDSV_IRQ  14U
#define SYSTICK_IRQ 15U

typedef struct _intr_handler_t {
	struct _intr_handler_t *next;
	struct _intr_handler_t *prev;
	unsigned int n;
	intrFn_t f;
	void *data;
	void *got;
} intr_handler_t;

#endif
