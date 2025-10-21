/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * iMXRT basic peripherals control functions
 *
 * Copyright 2019-2022 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_IMXRT1170_H_
#define _PH_HAL_IMXRT1170_H_


#include "hal/types.h"


void _imxrt_wdgReload(void);


int _imxrt_setIOmux(int mux, int sion, int mode);


int _imxrt_setIOpad(int pad, u8 sre, u8 dse, u8 pue, u8 pus, u8 ode, u8 apc);


int _imxrt_setIOisel(int isel, int daisy);


int _imxrt_setDevClock(int clock, int div, int mux, int mfd, int mfn, int state);


int _imxrt_setDirectLPCG(int clock, int state);


int _imxrt_getDirectLPCG(int clock, int *state);


int _imxrt_setLevelLPCG(int clock, int level);


void _imxrt_platformInit(void);


void _imxrt_init(void);


#endif
