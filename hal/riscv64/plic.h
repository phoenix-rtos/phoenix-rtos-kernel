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

#include "hal.h"


extern void plic_priority(unsigned int n, unsigned int priority);


extern u32 plic_priorityGet(unsigned int n);


extern int plic_isPending(unsigned int n);


extern void plic_tresholdSet(unsigned int hart, unsigned int priority);


extern u32 plic_tresholdGet(unsigned int hart);


extern unsigned int plic_claim(unsigned int hart);


extern int plic_complete(unsigned int hart, unsigned int n);


extern int plic_enableInterrupt(unsigned int hart, unsigned int n, char enable);


extern int _plic_init(void);


#endif
