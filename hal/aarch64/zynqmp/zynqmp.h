/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for ZynqMP
 *
 * Copyright 2022, 2024 Phoenix Systems
 * Author: Maciej Purski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ZYNQ_H_
#define _HAL_ZYNQ_H_

#include "hal/cpu.h"
#include "include/arch/aarch64/zynqmp/zynqmp.h"


extern int _zynqmp_setMIO(unsigned pin, u8 l0, u8 l1, u8 l2, u8 l3, u8 config);


extern int _zynq_setDevRst(int dev, unsigned int state);


#endif
