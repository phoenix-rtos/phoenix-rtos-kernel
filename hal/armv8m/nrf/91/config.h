/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for nRF9160
 *
 * Copyright 2021, 2022 Phoenix Systems
 * Author: Hubert Buczynski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_CONFIG_H_
#define _PH_HAL_CONFIG_H_


#ifndef __ASSEMBLY__

#include "include/arch/armv8m/nrf/syspage.h"
#include "nrf91.h"

/* Based on INTLINESNUM value (ICTR cpu register) */
#define SIZE_INTERRUPTS 256U

#define HAL_NAME_PLATFORM "NRF91 "
#endif

#endif
