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


extern int _zynqmp_setMIO(unsigned pin, char l0, char l1, char l2, char l3, char config);


extern int _zynq_setDevRst(int dev, unsigned int state);


#endif
