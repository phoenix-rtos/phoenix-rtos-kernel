/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for riscv64
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_CONFIG_H_
#define _PH_HAL_CONFIG_H_

#ifndef __ASSEMBLY__

#include "include/arch/riscv64/syspage.h"
#include "include/syspage.h"


#define AHB_IOAREA 0xfff00000U

#define PLIC_CONTEXTS_PER_HART 4U

#endif

#endif
