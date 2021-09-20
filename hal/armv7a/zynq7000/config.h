/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for Zynq 7000
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

/* TODO: temp solution, defines should be located in appropriate headers */
#define SYSTICK_IRQ 42

#define TIMER_US2CYC(x) ((55555LL * (x)) / 1000LL)
#define TIMER_CYC2US(x) (((x) * 1000LL) / 55555LL)

#define ADDR_DDR 0x00100000
#define SIZE_DDR 0x7ffffff

#ifndef __ASSEMBLY__

#include "../../include/arch/syspage-zynq7000.h"

#endif

#endif
