/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for STM32N6
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


#ifndef __ASSEMBLY__
#include "hal/types.h"
#include "include/arch/armv8m/stm32/syspage.h"
#include "include/syspage.h"
#include "hal/armv8m/stm32/stm32.h"
#include "hal/armv8m/stm32/stm32-timer.h"

#define SIZE_INTERRUPTS 211

#define HAL_NAME_PLATFORM "STM32N6 "

#endif

#endif
