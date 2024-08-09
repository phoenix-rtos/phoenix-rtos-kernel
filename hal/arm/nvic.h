/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Nested Vector Interrupt Controller
 *
 * Copyright 2017, 2020, 2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Hubert Buczynski, Damian Loewnau, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef HAL_ARMV_NVIC_H_
#define HAL_ARMV_NVIC_H_


#include "hal/types.h"


void _hal_nvicSetIRQ(s8 irqn, u8 state);


void _hal_nvicSetPriority(s8 irqn, u32 priority);


void _hal_nvicSetPending(s8 irqn);


void _hal_nvicInit(void);


#endif
