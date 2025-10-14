/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon-generic
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_GENERIC_H_
#define _HAL_GENERIC_H_


#ifndef __ASSEMBLY__


#include "hal/types.h"
#include <board_config.h>

#include "hal/gaisler/ambapp.h"

#include "include/arch/sparcv8leon/generic/generic.h"


int gaisler_setIomuxCfg(u8 pin, u8 opt, u8 pullup, u8 pulldn);


int hal_platformctl(void *ptr);


#endif /* __ASSEMBLY__ */


#endif
