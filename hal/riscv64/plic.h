/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * RISCV64 PLIC interrupt controler driver
 *
 * Copyright 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_PLIC_H_
#define _HAL_PLIC_H_

#include "hal/types.h"


void plic_priority(unsigned int n, unsigned int priority);


u32 plic_priorityGet(unsigned int n);


int plic_isPending(unsigned int n);


void plic_tresholdSet(unsigned int context, unsigned int priority);


u32 plic_tresholdGet(unsigned int context);


unsigned int plic_claim(unsigned int context);


void plic_complete(unsigned int context, unsigned int n);


int plic_enableInterrupt(unsigned int context, unsigned int n);


int plic_disableInterrupt(unsigned int context, unsigned int n);


void _plic_init(void);


#endif
