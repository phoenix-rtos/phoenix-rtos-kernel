/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for MCXN94x
 *
 * Copyright 2021, 2022, 2024 Phoenix Systems
 * Copyright 2026 Apator Metrix
 * Author: Hubert Buczynski, Damian Loewnau, Aleksander Kaminski, Mateusz Karcz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_CONFIG_H_
#define _PH_HAL_CONFIG_H_

#define SIZE_INTERRUPTS 172U

#define NUM_CPUS 1

#ifdef __ASSEMBLY__

#define CORTEXM33_PLATFORM_INIT _mcxn94x_init

#else

#include "include/arch/armv8m/mcx/syspage.h"
#include "mcxn94x.h"

#define HAL_NAME_PLATFORM "MCX N94x "
#endif

#endif
