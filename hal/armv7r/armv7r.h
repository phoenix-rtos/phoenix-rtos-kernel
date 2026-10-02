/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ARMv7 Cortex-R related routines
 *
 * Copyright 2021, 2024 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_ARMV7R_H_
#define _PH_HAL_ARMV7R_H_

#include <arch/cache.h>
#include "hal/types.h"


/* Barriers */


static inline void hal_cpuDataSyncBarrier(void)
{
	__asm__ volatile("dsb");
}


/* parasoft-begin-suppress MISRAC2012-RULE_8_6 "Each function has definition in assembly code" */


/* Core Management */

u32 hal_cpuGetMIDR(void);


u32 hal_cpuGetPFR0(void);


u32 hal_cpuGetPFR1(void);

/* parasoft-end-suppress MISRAC2012-RULE_8_6*/

#endif
