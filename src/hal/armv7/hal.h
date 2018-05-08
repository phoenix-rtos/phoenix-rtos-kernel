/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer (ARMv7)
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_HAL_H_

#ifndef __ASSEMBLY__

#include "syspage.h"
#include "cpu.h"
#include "string.h"
#include "console.h"
#include "pmap.h"
#include "spinlock.h"
#include "interrupts.h"
#include "exceptions.h"

#ifdef CPU_STM32
#include "stm32.h"
#endif

#ifdef CPU_IMXRT
#include "imxrt.h"
#endif


extern int hal_started(void);


extern void _hal_start(void);


extern void _hal_init(void);


#endif

#endif
