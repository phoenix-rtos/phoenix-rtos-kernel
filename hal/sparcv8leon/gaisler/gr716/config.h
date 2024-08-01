/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for sparcv8leon-gr716
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CONFIG_H_
#define _HAL_CONFIG_H_


#ifndef __ASSEMBLY__


#include "gr716.h"

#include "include/arch/sparcv8leon/syspage.h"
#include "include/syspage.h"

#define HAL_NAME_PLATFORM "SPARCv8 LEON3-GR716"


#endif /* __ASSEMBLY__ */


#define NWINDOWS 31
#define NUM_CPUS 1


#endif
