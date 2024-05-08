/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for STM32
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CONFIG_H_
#define _HAL_CONFIG_H_


#ifndef __ASSEMBLY__
#include "hal/types.h"
#include "include/arch/armv7m/stm32/syspage.h"
#include "include/syspage.h"
#include "stm32.h"
#include "stm32-timer.h"

#define HAL_NAME_PLATFORM "STM32 "
#endif

#endif
