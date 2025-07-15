/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for TDA4VM
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


#define NUM_CPUS 1

#ifndef __ASSEMBLY__

#define HAL_NAME_PLATFORM "TI K3 J721e SoC "

#include "include/arch/armv7r/tda4vm/syspage.h"
#include "include/syspage.h"

#endif

#endif
