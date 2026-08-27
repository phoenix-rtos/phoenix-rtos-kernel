/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for STM32H5
 *
 * Copyright 2021, 2025, 2026 Phoenix Systems
 * Author: Hubert Buczynski, Jacek Maksymowicz, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_CONFIG_H_
#define _PH_HAL_CONFIG_H_


#ifndef __ASSEMBLY__
#include "hal/types.h"
#include "include/arch/armv8m/stm32/syspage.h"
#include "include/syspage.h"
#include "hal/armv8m/stm32/stm32.h"

#define SIZE_INTERRUPTS 151U

/* Constants for configuring which TIM peripheral is used as system timer */
#define TIM_SYSTEM_BASE ((void *)0x50014800UL) /* TIM17 base address */
#define TIM_SYSTEM_PCTL pctl_tim17
#define TIM_SYSTEM_IRQ  tim17_irq
#define TIM_SYSTEM_FREQ (250UL * 1000000UL) /* Frequency in Hz */

#define HAL_NAME_PLATFORM "STM32H5 "

#endif

#endif
