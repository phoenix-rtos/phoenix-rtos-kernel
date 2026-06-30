/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Extra timer interface for stm32
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_STM32TIMER_H_
#define _PH_HAL_STM32TIMER_H_

#include "hal/types.h"


void timer_jiffiesAdd(time_t t);


void timer_setAlarm(time_t us);


#endif