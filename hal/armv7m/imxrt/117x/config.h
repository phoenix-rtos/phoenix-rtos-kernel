/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for i.MX RT117x
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

#define SIZE_INTERRUPTS 217

#define TIMER_US2CYC(x) (x)
#define TIMER_CYC2US(x) (x)

#ifndef __ASSEMBLY__
#include "imxrt117x.h"
#include "../../include/arch/syspage-imxrt.h"
#endif

#endif
