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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_CONFIG_H_
#define _PH_HAL_CONFIG_H_

#define ADDR_DDR 0x80000000U
#define SIZE_DDR 0x7ffffffU

#define NUM_CPUS 1

#define EPIT1_BASE 0x020d0000U
#define EPIT1_IRQ  88U
#define EPIT2_BASE 0x020d4000U
#define EPIT2_IRQ  89U

#define GPT1_BASE 0x02098000U
#define GPT1_IRQ  87U

#define GPT_BASE     GPT1_BASE
#define GPT_IRQ      GPT1_IRQ
#define EPIT_BASE    EPIT1_BASE
#define TIMER_IRQ_ID EPIT1_IRQ

#ifndef __ASSEMBLY__

#define HAL_NAME_PLATFORM "NXP i.MX 6ULL "

#include "include/arch/armv7a/imx6ull/syspage.h"
#include "include/syspage.h"

#endif


#endif
