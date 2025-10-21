/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * RISCV64 PLIC interrupt controller driver
 *
 * Copyright 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_PLIC_H_
#define _PH_HAL_PLIC_H_

#include "hal/types.h"
#include <config.h>


/* PLIC Supervisor Context number */
#define PLIC_SCONTEXT(hartId) (PLIC_CONTEXTS_PER_HART * (hartId) + 1U)


u32 plic_read(unsigned int reg);


void plic_write(unsigned int reg, u32 v);


void plic_priority(unsigned int n, unsigned int priority);


u32 plic_priorityGet(unsigned int n);


int plic_isPending(unsigned int n);


void plic_tresholdSet(unsigned int context, unsigned int priority);


u32 plic_tresholdGet(unsigned int context);


unsigned int plic_claim(unsigned int context);


void plic_complete(unsigned int context, unsigned int n);


int plic_enableInterrupt(unsigned int context, unsigned int n);


int plic_disableInterrupt(unsigned int context, unsigned int n);


void plic_initCore(void);


void plic_init(void);


#endif
