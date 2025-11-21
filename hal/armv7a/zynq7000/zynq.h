/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for zynq7000
 *
 * Copyright 2022 Phoenix Systems
 * Author: Maciej Purski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_ZYNQ_H_
#define _PH_HAL_ZYNQ_H_

#include "hal/cpu.h"


int _zynq_setMIO(int pin, u8 disableRcvr, u8 pullup, u8 ioType, u8 speed, u8 l0, u8 l1, u8 l2, u8 l3, u8 triEnable);


int _zynq_setAmbaClk(u32 dev, u8 state);


void _zynq_interrupts_setCPU(unsigned int irqn, u32 cpuID);


#endif
