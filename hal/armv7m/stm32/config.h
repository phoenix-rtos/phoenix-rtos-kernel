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

#define TIMER_US2CYC(x) (x)
#define TIMER_CYC2US(x) (x)

#ifndef __ASSEMBLY__
#include "stm32.h"
#include "stm32-timer.h"
#include "../../include/arch/syspage-stm32.h"
#endif

#endif
