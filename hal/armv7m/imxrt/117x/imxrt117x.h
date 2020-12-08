/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * iMXRT basic peripherals control functions
 *
 * Copyright 2019 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_IMXRT1170_H_
#define _HAL_IMXRT1170_H_

#include "cpu.h"
#include "pmap.h"
#include "spinlock.h"


static inline void imxrt_dataSyncBarrier(void)
{
	__asm__ volatile ("dsb");
}


static inline void imxrt_dataInstrBarrier(void)
{
	__asm__ volatile ("isb");
}


static inline void imxrt_dataBarrier(void)
{
	__asm__ volatile ("dmb");
}


extern int hal_platformctl(void *ptr);


extern unsigned int _imxrt_cpuid(void);


extern void _imxrt_nvicSetIRQ(s8 irqn, u8 state);


extern void _imxrt_nvicSetPriority(s8 irqn, u32 priority);


extern void _imxrt_scbSetPriorityGrouping(u32 group);


extern void _imxrt_scbSetPriority(s8 excpn, u32 priority);


extern void _imxrt_wdgReload(void);


extern int _imxrt_setIOmux(int mux, char sion, char mode);


extern int _imxrt_setIOpad(int pad, char sre, char dse, char pue, char pus, char ode, char apc);


extern int _imxrt_setIOisel(int isel, char daisy);


extern void _imxrt_platformInit(void);


extern void _imxrt_init(void);


extern int _imxrt_systickInit(u32 interval);


extern void _imxrt_systickSet(u8 state);


extern u32 _imxrt_systickGet(void);


extern void _imxrt_nvicSetIRQ(s8 irqn, u8 state);


extern void _imxrt_nvicSystemReset(void);


#endif
