/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for MCXN94x
 *
 * Copyright 2021, 2022, 2024 Phoenix Systems
 * Author: Hubert Buczynski, Damian Loewnau, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CONFIG_H_
#define _HAL_CONFIG_H_

#define SIZE_INTERRUPTS (171 + 16)

#ifndef __ASSEMBLY__
#include "hal/types.h"
#include "include/arch/armv8m/mcx/syspage.h"
#include "include/syspage.h"
#include "mcxn94x.h"

// #MPUTEST: GPIO CONFIG
#ifndef MPUTEST_PORT0
#define MPUTEST_PORT0 3
#endif
#ifndef MPUTEST_PIN0
#define MPUTEST_PIN0 21
#endif
#ifndef MPUTEST_PORT1
#define MPUTEST_PORT1 3
#endif
#ifndef MPUTEST_PIN1
#define MPUTEST_PIN1 16
#endif

#define MPUTEST_GPIO_CLR(port, pin) *((u8 *)(0x40096060U + 0x2000 * port) + pin) = 0; /* set output value */
#define MPUTEST_GPIO_SET(port, pin) *((u8 *)(0x40096060U + 0x2000 * port) + pin) = 1; /* set output value */

#define HAL_NAME_PLATFORM "MCX N94x "
#endif

#endif
