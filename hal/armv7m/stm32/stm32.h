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

#ifndef _PH_HAL_STM32_H_
#define _PH_HAL_STM32_H_

#include "hal/types.h"
#include "hal/pmap.h"

#ifdef __CPU_STM32L4X6
#include "include/arch/armv7m/stm32/l4/stm32l4.h"
#endif


void _stm32_platformInit(void);


/* Sets peripheral clock */
int _stm32_rccSetDevClock(unsigned int d, u32 state);


/* Sets CPU clock to the closest smaller MSI frequency */
int _stm32_rccSetCPUClock(u32 hz);


int _stm32_rccGetDevClock(unsigned int d, u32 *state);


u32 _stm32_rccGetCPUClock(void);


void _stm32_rccClearResetFlags(void);


int _stm32_gpioConfig(unsigned int d, u8 pin, u8 mode, u8 af, u8 otype, u8 ospeed, u8 pupd);


int _stm32_gpioSet(unsigned int d, u8 pin, u8 val);


int _stm32_gpioSetPort(unsigned int d, u16 val);


int _stm32_gpioGet(unsigned int d, u8 pin, u8 *val);


int _stm32_gpioGetPort(unsigned int d, u16 *val);


/* Range = 0 - forbidden, 1 - 1.8V, 2 - 1.5V, 3 - 1.2V */
void _stm32_pwrSetCPUVolt(u8 range);


void _stm32_pwrEnterLPRun(u32 state);


time_t _stm32_pwrEnterLPStop(time_t us);


void _stm32_rtcUnlockRegs(void);


void _stm32_rtcLockRegs(void);


u32 _stm32_rtcGetms(void);


void _stm32_scbSetPriorityGrouping(u32 group);


u32 _stm32_scbGetPriorityGrouping(void);


void _stm32_scbSetPriority(s8 excpn, u32 priority);


u32 _stm32_scbGetPriority(s8 excpn);


void _stm32_nvicSetIRQ(s8 irqn, u8 state);


u32 _stm32_nvicGetPendingIRQ(s8 irqn);


void _stm32_nvicSetPendingIRQ(s8 irqn, u8 state);


u32 _stm32_nvicGetActive(s8 irqn);


void _stm32_nvicSetPriority(s8 irqn, u32 priority);


u8 _stm32_nvicGetPriority(s8 irqn);


void _stm32_nvicSystemReset(void);


int _stm32_extiMaskInterrupt(u32 line, u8 state);


int _stm32_extiMaskEvent(u32 line, u8 state);


int _stm32_extiSetTrigger(u32 line, u8 state, u8 edge);


int _stm32_syscfgExtiLineConfig(u8 port, u8 pin);


int _stm32_extiSoftInterrupt(u32 line);


u32 _stm32_extiGetPending(void);


int _stm32_extiClearPending(u32 line);


int _stm32_systickInit(u32 interval);


void _stm32_systickSet(u8 state);


u32 _stm32_systickGet(void);


unsigned int _stm32_cpuid(void);


void _stm32_wdgReload(void);


void _stm32_init(void);


#endif
