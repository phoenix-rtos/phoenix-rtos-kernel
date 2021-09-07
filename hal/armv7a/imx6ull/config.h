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

#define ADDR_OCRAM 0x907000
#define ADDR_DDR   0x80000000
#define SIZE_DDR   0x7ffffff


#ifndef __ASSEMBLY__

#include "../../include/arch/syspage-imx6ull.h"

#endif


#endif
