/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for sparcv8leon3-gr716
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

#include "include/arch/syspage-sparcv8leon3.h"
#include "include/syspage.h"

#define HAL_NAME_PLATFORM "SPARCv8 LEON3-GR716"


#endif /* __ASSEMBLY__ */


#define NWINDOWS      31
#define INT_CTRL_BASE ((void *)0x80002000)


#endif
