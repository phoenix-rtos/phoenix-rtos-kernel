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

/* Constants for configuring which LPTIM peripheral is used as system timer */
#define LPTIM_SYSTEM_BASE      ((void *)0x50009400UL) /* LPTIM2 base address */
#define LPTIM_SYSTEM_IRQ       lptim2_irq
#define LPTIM_SYSTEM_PCTL      pctl_lptim2
#define LPTIM_SYSTEM_IPCLK_SEL pctl_ipclk_lptim2sel
#define LPTIM_SYSTEM_IPCLK_VAL 1       /* LSI */
#define LPTIM_SYSTEM_INPUT     32000UL /* LSI input frequency in Hz */
#define LPTIM_SYSTEM_CYCLE_MS  16000UL /* 16 s */

#define HAL_NAME_PLATFORM "STM32U3 "

#endif

#endif
