/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for STM32U3
 *
 * Copyright 2021, 2025 Phoenix Systems
 * Copyright 2026 Apator Metrix
 * Author: Hubert Buczynski, Jacek Maksymowicz, Mateusz Karcz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_CONFIG_H_
#define _PH_HAL_CONFIG_H_


#define SIZE_INTERRUPTS 141U

#define NUM_CPUS 1

#ifdef __ASSEMBLY__

#define CORTEXM33_PLATFORM_INIT _stm32_init

#else

#include "hal/types.h"
#include "include/arch/armv8m/stm32/syspage.h"
#include "include/syspage.h"
#include "hal/armv8m/stm32/stm32.h"
#include "hal/armv8m/stm32/stm32-timer.h"

/* Constants for configuring which TIM peripheral is used as system timer */
#define TIM_SYSTEM_BASE ((void *)0x50001400UL) /* TIM7 base address */
#define TIM_SYSTEM_PCTL pctl_tim7
#define TIM_SYSTEM_IRQ  tim7_irq
#define TIM_SYSTEM_FREQ (12UL * 1000000UL) /* Frequency in Hz */

#define HAL_NAME_PLATFORM "STM32U3 "

#endif

#endif
