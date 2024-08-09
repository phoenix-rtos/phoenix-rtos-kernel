/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System Control Block
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


void _hal_scbSetPriorityGrouping(u32 group);


void _hal_scbSetPriority(s8 excpn, u32 priority);


void _hal_scbSystemReset(void);


unsigned int _hal_scbCpuid(void);


void _hal_scbSetFPU(int state);


void _hal_scbEnableDCache(void);


void _hal_scbDisableDCache(void);


void _hal_scbCleanInvalDCacheAddr(void *addr, u32 sz);


void _hal_scbEnableICache(void);


void _hal_scbDisableICache(void);


void _hal_scbSetDeepSleep(int state);


void _hal_scbSystickInit(u32 load);


void _hal_scbInit(void);


#endif
