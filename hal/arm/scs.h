/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Cortex-M System Control Space
 *
 * Copyright 2017, 2020, 2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Hubert Buczynski, Damian Loewnau, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef HAL_ARMV_SCB_H_
#define HAL_ARMV_SCB_H_


#include "hal/types.h"


void _hal_scsIRQSet(s8 irqn, u8 state);


void _hal_scsIRQPrioritySet(s8 irqn, u32 priority);


void _hal_scsIRQPendingSet(s8 irqn);


int _hal_scsIRQPendingGet(s8 irqn);


int _hal_scsIRQActiveGet(s8 irqn);


void _hal_scsPriorityGroupingSet(u32 group);


u32 _hal_scsPriorityGroupingGet(void);


void _hal_scsExceptionPrioritySet(s8 excpn, u32 priority);


u32 _imxrt_scsExceptionPriorityGet(s8 excpn);


void _hal_scsSystemReset(void);


unsigned int _hal_scsCpuID(void);


void _hal_scsFPUSet(int state);


void _hal_scsDCacheEnable(void);


void _hal_scsDCacheDisable(void);


void _hal_scsDCacheCleanInvalAddr(void *addr, u32 sz);


void _hal_scsDCacheCleanAddr(void *addr, u32 sz);


void _hal_scsDCacheInvalAddr(void *addr, u32 sz);


void _hal_scsICacheEnable(void);


void _hal_scsICacheDisable(void);


void _hal_scsDeepSleepSet(int state);


void _hal_scsSystickInit(u32 load);


/* Reads the SysTick current value and returns it.
 * If `overflow_out` is not NULL, also reads and clears the timer overflow flag.
 * If overflow has occurred, the returned timestamp is guaranteed to be
 * after the overflow. */
u32 _hal_scsSystickGetCount(u8 *overflow_out);


/* Get the value to use for FPSCR when creating a new context.
 * It is stored in FPDSCR register. */
u32 _hal_scsGetDefaultFPSCR(void);


void _hal_scsInit(void);


#endif
