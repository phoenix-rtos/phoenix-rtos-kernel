/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for ia2
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

#define CPU_SUPPORTS_TIMER_WAKEUP 1 /* if the function hal_timerSetWakeup is supported on this architecture */

#ifndef __ASSEMBLY__

#include "include/arch/ia32/syspage.h"
#include "include/syspage.h"

#endif


#endif
