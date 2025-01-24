/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon-gr740
 *
 * Copyright 2025 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_GR740_H_
#define _HAL_GR740_H_


#ifndef __ASSEMBLY__


#include "hal/types.h"
#include <board_config.h>

#include "hal/gaisler/ambapp.h"
#include "hal/sparcv8leon/gaisler/gaisler.h"

#include "include/arch/sparcv8leon/gr740/gr740.h"


int gaisler_setIomuxCfg(u8 pin, u8 opt, u8 pullup, u8 pulldn);


void _gr740_cguClkEnable(u32 device);


void _gr740_cguClkDisable(u32 device);


int _gr740_cguClkStatus(u32 device);


int hal_platformctl(void *ptr);


void _hal_platformInit(void);


#endif /* __ASSEMBLY__ */


#endif
