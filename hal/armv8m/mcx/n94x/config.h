/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for MCXN94x
 *
 * Copyright 2021, 2022, 2024 Phoenix Systems
 * Author: Hubert Buczynski, Damian Loewnau, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_CONFIG_H_
#define _PH_HAL_CONFIG_H_

#define SIZE_INTERRUPTS (171U + 16U)

#ifndef __ASSEMBLY__

#include "include/arch/armv8m/mcx/syspage.h"
#include "mcxn94x.h"

#define HAL_NAME_PLATFORM "MCX N94x "
#endif

#endif
