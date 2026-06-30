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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_ZYNQ_H_
#define _PH_HAL_ZYNQ_H_

#include "hal/cpu.h"
#include "include/arch/armv7r/zynqmp/zynqmp.h"


int _zynqmp_setMIO(unsigned int pin, u8 l0, u8 l1, u8 l2, u8 l3, u8 config);


int _zynq_setDevRst(int dev, unsigned int state);


#endif
