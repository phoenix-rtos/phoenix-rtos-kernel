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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_CONFIG_H_
#define _PH_HAL_CONFIG_H_

#ifndef __ASSEMBLY__

#include "include/arch/riscv64/syspage.h"
#include "include/syspage.h"

#define PLIC_CONTEXTS_PER_HART 2

#endif

#endif
