/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for ZynqMP
 *
 * Copyright 2021, 2024 Phoenix Systems
 * Author: Hubert Buczynski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CONFIG_H_
#define _HAL_CONFIG_H_


/* On-Chip memory */
#define ADDR_OCRAM 0xfffc0000
#define SIZE_OCRAM (256 * 1024)

#define ASID_BITS       16
#define NUM_CPUS        4
#define SIZE_INTERRUPTS 188

#ifndef __ASSEMBLY__

#define HAL_NAME_PLATFORM "Xilinx Zynq Ultrascale+ "

#include "include/arch/aarch64/zynqmp/syspage.h"
#include "include/syspage.h"

#endif

#endif
