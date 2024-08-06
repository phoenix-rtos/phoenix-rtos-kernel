/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon3-gr712rc
 *
 * Copyright 2023 Phoenix Systems
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

#include "include/arch/sparcv8leon3/gr712rc/gr712rc.h"


int gaisler_setIomuxCfg(u8 pin, u8 opt, u8 pullup, u8 pulldn);


void _gr712rc_cguClkEnable(u32 device);


void _gr712rc_cguClkDisable(u32 device);


int _gr712rc_cguClkStatus(u32 device);


int hal_platformctl(void *ptr);


void _hal_platformInit(void);


#endif /* __ASSEMBLY__ */


#endif
