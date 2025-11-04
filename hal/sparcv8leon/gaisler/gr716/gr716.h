/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon-gr716
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_GR716_H_
#define _HAL_GR716_H_


#ifndef __ASSEMBLY__


#include "hal/types.h"
#include <board_config.h>

#include "hal/gaisler/ambapp.h"
#include "hal/sparcv8leon/gaisler/gaisler.h"

#include "include/arch/sparcv8leon/gr716/gr716.h"


#define TIMER_IRQ 9


int _gr716_getIomuxCfg(u8 pin, u8 *opt, u8 *pullup, u8 *pulldn);


void _gr716_cguClkEnable(u32 cgu, u32 device);


void _gr716_cguClkDisable(u32 cgu, u32 device);


int _gr716_cguClkStatus(u32 cgu, u32 device);


int hal_platformctl(void *ptr);


#endif /* __ASSEMBLY__ */


#endif
