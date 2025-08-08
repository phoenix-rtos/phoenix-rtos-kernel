/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * nRF91 basic peripherals control functions
 *
 * Copyright 2022 Phoenix Systems
 * Author: Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_NRF91_H_
#define _HAL_NRF91_H_

#include "hal/types.h"
#include "hal/pmap.h"

#include "include/arch/armv8m/nrf/91/nrf9160.h"


/* clang-format off */
enum { gpio_input = 0, gpio_output };
enum { gpio_low = 0, gpio_high };
enum { gpio_nopull = 0, gpio_pulldown, gpio_pullup = 3};


enum { scb_actlr = 2, scb_cpuid = 832, scb_icsr, scb_vtor, scb_aircr, scb_scr, scb_ccr, scb_shp1, scb_shp2,
	scb_shp3, scb_shcsr, scb_cfsr, scb_mmsr, scb_bfsr, scb_ufsr, scb_hfsr, scb_mmar, scb_bfar, scb_afsr };
/* clang-format on */


extern int hal_platformctl(void *);


extern int _nrf91_systickInit(u32 interval);


extern int _nrf91_gpioConfig(u8 pin, u8 dir, u8 pull);


extern int _nrf91_gpioSet(u8 pin, u8 val);


extern void _nrf91_scbSetPriorityGrouping(u32 group);


extern u32 _nrf91_scbGetPriorityGrouping(void);


extern void _nrf91_scbSetPriority(s8 excpn, u32 priority);


extern u32 _nrf91_scbGetPriority(s8 excpn);


extern unsigned int _nrf91_cpuid(void);


#endif
