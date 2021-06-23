/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * STM32 basic peripherals control functions
 *
 * Copyright 2017, 2019-2020 Phoenix Systems
 * Author: Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_STM32_H_
#define _HAL_STM32_H_

#include "../cpu.h"
#include "pmap.h"
#include "../spinlock.h"


#if defined(CPU_STM32L152XD) || defined(CPU_STM32L152XE)
#include "../../../include/arch/stm32l1.h"
#endif

#ifdef CPU_STM32L4X6
#include "../../../include/arch/stm32l4.h"
#endif



/* platformctl syscall */
extern int hal_platformctl(void *);


extern void _stm32_platformInit(void);


/* Sets peripheral clock */
extern int _stm32_rccSetDevClock(unsigned int d, u32 hz);


/* Sets CPU clock to the closest smaller MSI freqency */
extern int _stm32_rccSetCPUClock(u32 hz);


extern int _stm32_rccGetDevClock(unsigned int d, u32 *hz);


extern u32 _stm32_rccGetCPUClock(void);


extern void _stm32_rccClearResetFlags(void);


extern int _stm32_gpioConfig(unsigned int d, u8 pin, u8 mode, u8 af, u8 otype, u8 ospeed, u8 pupd);


extern int _stm32_gpioSet(unsigned int d, u8 pin, u8 val);


extern int _stm32_gpioSetPort(unsigned int d, u16 val);


extern int _stm32_gpioGet(unsigned int d, u8 pin, u8 *val);


extern int _stm32_gpioGetPort(unsigned int d, u16 *val);


/* Range = 0 - forbidden, 1 - 1.8V, 2 - 1.5V, 3 - 1.2V */
extern void _stm32_pwrSetCPUVolt(u8 range);


extern void _stm32_pwrEnterLPRun(u32 state);


extern time_t _stm32_pwrEnterLPStop(time_t ms);


extern void _stm32_rtcUnlockRegs(void);


extern void _stm32_rtcLockRegs(void);


extern u32 _stm32_rtcGetms(void);


extern void _stm32_scbSetPriorityGrouping(u32 group);


extern u32 _stm32_scbGetPriorityGrouping(void);


extern void _stm32_scbSetPriority(s8 excpn, u32 priority);


extern u32 _stm32_scbGetPriority(s8 excpn);


extern void _stm32_nvicSetIRQ(s8 irqn, u8 state);


extern u32 _stm32_nvicGetPendingIRQ(s8 irqn);


extern void _stm32_nvicSetPendingIRQ(s8 irqn, u8 state);


extern u32 _stm32_nvicGetActive(s8 irqn);


extern void _stm32_nvicSetPriority(s8 irqn, u32 priority);


extern u8 _stm32_nvicGetPriority(s8 irqn);


extern void _stm32_nvicSystemReset(void);


extern int _stm32_extiMaskInterrupt(u32 line, u8 state);


extern int _stm32_extiMaskEvent(u32 line, u8 state);


extern int _stm32_extiSetTrigger(u32 line, u8 state, u8 edge);


extern int _stm32_syscfgExtiLineConfig(u8 port, u8 pin);


extern int _stm32_extiSoftInterrupt(u32 line);


extern u32 _stm32_extiGetPending(void);


extern int _stm32_extiClearPending(u32 line);


extern int _stm32_systickInit(u32 interval);


extern void _stm32_systickSet(u8 state);


extern u32 _stm32_systickGet(void);


extern void _stm32_mpuReadRegion(u8 region, mpur_t *reg);


extern void _stm32_mpuEnableRegion(u8 region, u8 state);


extern void _stm32_mpuUpdateRegion(mpur_t *reg);


extern unsigned int _stm32_cpuid(void);


extern void _stm32_wdgReload(void);


extern int _stm32_systickInit(u32 interval);


extern void _stm32_init(void);


#endif
