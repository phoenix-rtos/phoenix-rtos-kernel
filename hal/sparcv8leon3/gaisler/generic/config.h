/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for sparcv8leon3-generic
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CONFIG_H_
#define _HAL_CONFIG_H_


#ifndef __ASSEMBLY__


#include "generic.h"

#include "include/arch/sparcv8leon3/syspage.h"
#include "include/syspage.h"

#define HAL_NAME_PLATFORM "SPARCv8 LEON3-Generic"

#define ADDR_RAM 0x40000000
#define SIZE_RAM (128 * 1024 * 1024) /* 128 MB */


#endif /* __ASSEMBLY__ */


#define NWINDOWS 8
#define NUM_CPUS 1


#endif
