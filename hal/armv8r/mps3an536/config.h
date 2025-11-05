/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for ARMv8-R MPS3 AN536
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_CONFIG_H_
#define _PH_HAL_CONFIG_H_

#define NUM_CPUS 1

#ifndef __ASSEMBLY__

#define HAL_NAME_PLATFORM "MPS3 AN536 "

#include "include/arch/armv8r/mps3an536/syspage.h"
#include "include/syspage.h"

#endif


#endif
