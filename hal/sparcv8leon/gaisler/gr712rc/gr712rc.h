/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL internal functions for sparcv8leon-gr712rc
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_GR716_H_
#define _PH_HAL_GR716_H_


#ifndef __ASSEMBLY__


#include "hal/types.h"
#include <board_config.h>

#include "hal/gaisler/ambapp.h"
#include "hal/sparcv8leon/gaisler/gaisler.h"

#include "include/arch/sparcv8leon/gr712rc/gr712rc.h"


void _gr712rc_cguClkEnable(u32 device);


void _gr712rc_cguClkDisable(u32 device);


int _gr712rc_cguClkStatus(u32 device);

#endif /* __ASSEMBLY__ */


#endif
