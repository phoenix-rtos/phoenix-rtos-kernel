/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for i.MX 6ULL
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
#define HPTIMER_IRQ 88

/* IMX6ULL timer frequency set to 66 MHz */
#define TIMER_US2CYC(x) (66LL * (x))
#define TIMER_CYC2US(x) ((x) / 66)

#define ADDR_OCRAM 0x907000
#define ADDR_DDR   0x80000000
#define SIZE_DDR   0x7ffffff


#ifndef __ASSEMBLY__

#define HAL_NAME_PLATFORM "NXP i.MX 6ULL "

#include "../../include/arch/syspage-imx6ull.h"

#endif


#endif
