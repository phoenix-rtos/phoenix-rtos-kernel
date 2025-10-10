/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * MCXN94X basic peripherals control functions
 *
 * Copyright 2024 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_MCXN94X_H_
#define _HAL_MCXN94X_H_

#include "hal/types.h"
#include "hal/pmap.h"

#include "include/arch/armv8m/mcx/n94x/mcxn94x.h"


extern int _mcxn94x_portPinConfig(int pin, int mux, int options);


extern u64 _mcxn94x_sysconGray2Bin(u64 gray);


extern int _mcxn94x_sysconSetDevClk(int dev, unsigned int sel, unsigned int div, int enable);


extern int _mcxn94x_sysconDevReset(int dev, int state);


extern int hal_platformctl(void *ptr);


extern void _mcxn94x_scbSetPriorityGrouping(u32 group);


extern void _mcxn94x_scbSetPriority(s8 excpn, u32 priority);


extern unsigned int _mcxn94x_cpuid(void);

#endif
