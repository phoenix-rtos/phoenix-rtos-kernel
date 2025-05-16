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

#ifndef _PH_HAL_CONFIG_H_
#define _PH_HAL_CONFIG_H_


/* On-Chip memory */
#define ADDR_OCRAM_LOW  0x00000000U
#define SIZE_OCRAM_LOW  192U * 1024U
#define ADDR_OCRAM_HIGH 0xffff0000U
#define SIZE_OCRAM_HIGH 64U * 1024U

/* TODO: temp solution, defines describe specific platform */
#define ADDR_DDR 0x00100000U
#define SIZE_DDR 0x7ffffffU

#define NUM_CPUS 2

#define TIMER_IRQ_ID 42U

#ifndef __ASSEMBLY__

#define HAL_NAME_PLATFORM "Xilinx Zynq-7000 "

#include "include/arch/armv7a/zynq7000/syspage.h"
#include "include/syspage.h"

#endif

#endif
