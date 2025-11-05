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

#ifndef _PH_HAL_CONFIG_H_
#define _PH_HAL_CONFIG_H_


#ifndef __ASSEMBLY__
#include "hal/types.h"
#include "include/arch/armv8m/stm32/syspage.h"
#include "include/syspage.h"
#include "hal/armv8m/stm32/stm32.h"
#include "hal/armv8m/stm32/stm32-timer.h"

#define SIZE_INTERRUPTS 211

/* Constants for configuring which TIM peripheral is used as system timer */
#define TIM_SYSTEM_BASE ((void *)0x52003C00) /* TIM18 base address */
#define TIM_SYSTEM_PCTL pctl_tim18
#define TIM_SYSTEM_IRQ  tim18_irq
#define TIM_SYSTEM_FREQ (400UL * 1000000UL) /* Frequency in Hz */

#define HAL_NAME_PLATFORM "STM32N6 "

#endif

#endif
