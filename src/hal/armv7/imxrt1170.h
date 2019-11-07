/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * iMXRT basic peripherals control functions
 *
 * Copyright 2019 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_IMXRT1170_H_
#define _HAL_IMXRT1170_H_

#include "cpu.h"
#include "pmap.h"
#include "spinlock.h"


extern int hal_platformctl(void *ptr);


extern unsigned int _imxrt_cpuid(void);


extern void _imxrt_wdgReload(void);


extern int _imxrt_setIOmux(int mux, char sion, char mode);


extern int _imxrt_setIOpad(int pad, char sre, char dse, char pue, char pus, char ode, char apc);


extern int _imxrt_setIOisel(int isel, char daisy);


extern void _imxrt_platformInit(void);


extern void _imxrt_init(void);


#endif
