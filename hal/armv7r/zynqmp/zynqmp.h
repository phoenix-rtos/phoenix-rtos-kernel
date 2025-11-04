/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for ZynqMP
 *
 * Copyright 2022, 2025 Phoenix Systems
 * Author: Maciej Purski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ZYNQ_H_
#define _HAL_ZYNQ_H_

#include "hal/cpu.h"
#include "include/arch/armv7r/zynqmp/zynqmp.h"


int _zynqmp_setMIO(unsigned int pin, u8 l0, u8 l1, u8 l2, u8 l3, u8 config);


int _zynq_setDevRst(int dev, unsigned int state);


#endif
