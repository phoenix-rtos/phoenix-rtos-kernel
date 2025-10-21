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

#ifndef _PH_HAL_MCXN94X_H_
#define _PH_HAL_MCXN94X_H_

#include "hal/types.h"
#include "hal/pmap.h"

#include "include/arch/armv8m/mcx/n94x/mcxn94x.h"


int _mcxn94x_portPinConfig(int pin, int mux, int options);


u64 _mcxn94x_sysconGray2Bin(u64 gray);


int _mcxn94x_sysconSetDevClk(int dev, unsigned int sel, unsigned int div, int enable);


int _mcxn94x_sysconDevReset(int dev, int state);


#endif
