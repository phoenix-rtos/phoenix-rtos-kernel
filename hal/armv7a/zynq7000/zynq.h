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

#ifndef _HAL_ZYNQ_H_
#define _HAL_ZYNQ_H_

#include "../../cpu.h"


extern int _zynq_setMIO(unsigned int pin, char disableRcvr, char pullup, char ioType, char speed, char l0, char l1, char l2, char l3, char triEnable);


extern int _zynq_setAmbaClk(u32 dev, u32 state);


#endif
