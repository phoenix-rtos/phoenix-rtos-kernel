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
#include "timer.h"

#if defined(CPU_STM32L152XD) || defined(CPU_STM32L152XE) || defined(CPU_STM32L4X6)
#include "stm32.h"
#endif

#if defined(CPU_IMXRT105X) || defined(CPU_IMXRT106X)
#include "imxrt10xx.h"
#endif

#ifdef CPU_IMXRT117X
#include "imxrt117x.h"
#endif


extern int hal_started(void);


extern void _hal_start(void);


extern void _hal_init(void);


#endif

#endif
