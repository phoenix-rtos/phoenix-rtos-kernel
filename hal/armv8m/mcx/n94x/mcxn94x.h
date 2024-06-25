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


enum {
	scb_actlr = 2, scb_cpuid = 832, scb_icsr, scb_vtor, scb_aircr, scb_scr, scb_ccr, scb_shp1, scb_shp2,
	scb_shp3, scb_shcsr, scb_cfsr, scb_mmsr, scb_bfsr, scb_ufsr, scb_hfsr, scb_mmar, scb_bfar, scb_afsr
};


extern int _mcxn94x_sysconSetDevClk(int dev, unsigned int sel, unsigned int div, int enable);


extern int hal_platformctl(void *);


extern void _hal_platformInit(void);


extern void _mcxn94x_scbSetPriorityGrouping(u32 group);


extern void _mcxn94x_scbSetPriority(s8 excpn, u32 priority);

#endif