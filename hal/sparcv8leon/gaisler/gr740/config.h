/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for sparcv8leon-gr740
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


#ifndef __ASSEMBLY__


#include "gr740.h"

#include "include/arch/sparcv8leon/syspage.h"
#include "include/syspage.h"

#define HAL_NAME_PLATFORM "SPARCv8 LEON3-GR740"

#define ADDR_RAM 0x00000000U
#define SIZE_RAM (128U * 1024U * 1024U) /* 128 MB */

#define L2CACHE_CTRL_BASE ((void *)0xf0000000U)

#endif /* __ASSEMBLY__ */


#define NWINDOWS 8U
#define NUM_CPUS 4U


#endif
