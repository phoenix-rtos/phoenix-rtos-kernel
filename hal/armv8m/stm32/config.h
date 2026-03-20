/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file dispatch for STM32 ARMv8-M MCUs
 *
 * Copyright 2026 Apator Metrix
 * Author: Mateusz Karcz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_CONFIG_DISPATCH_H_
#define _PH_HAL_CONFIG_DISPATCH_H_

#if defined(__CPU_STM32N6)
#include "n6/config.h"
#elif defined(__CPU_STM32U3)
#include "u3/config.h"
#else
#error "Unsupported platform"
#endif

#endif
