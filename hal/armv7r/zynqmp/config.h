/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for ZynqMP
 *
 * Copyright 2021, 2025 Phoenix Systems
 * Author: Hubert Buczynski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CONFIG_H_
#define _HAL_CONFIG_H_


#define NUM_CPUS        1
#define SIZE_INTERRUPTS 188

#ifndef __ASSEMBLY__

#define HAL_NAME_PLATFORM "Xilinx Zynq Ultrascale+ "

#include "include/arch/armv7r/zynqmp/syspage.h"
#include "include/syspage.h"

#endif

#endif
