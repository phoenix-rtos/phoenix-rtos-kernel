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

#ifndef _HAL_IMXRT1170_H_
#define _HAL_IMXRT1170_H_


#include "hal/types.h"


extern int hal_platformctl(void *ptr);


extern void _imxrt_wdgReload(void);


extern int _imxrt_setIOmux(int mux, char sion, char mode);


extern int _imxrt_setIOpad(int pad, char sre, char dse, char pue, char pus, char ode, char apc);


extern int _imxrt_setIOisel(int isel, char daisy);


extern int _imxrt_setDevClock(int clock, int div, int mux, int mfd, int mfn, int state);


extern int _imxrt_setDirectLPCG(int clock, int state);


extern int _imxrt_getDirectLPCG(int clock, int *state);


extern int _imxrt_setLevelLPCG(int clock, int level);


extern void _imxrt_platformInit(void);


extern void _imxrt_init(void);


extern void hal_invalICacheAll(void);
extern void hal_invalDCacheAll(void);
extern void testGPIOlatencyConfigure(void);  // #MPUTEST: TEST GPIO LATENCY
extern void testGPIOlatency(int cacheopt);   // #MPUTEST: TEST GPIO LATENCY


#endif
