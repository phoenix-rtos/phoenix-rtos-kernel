/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ARMv7 Cortex-M related routines
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ARMV7M_H_
#define _HAL_ARMV7M_H_


static inline void hal_cpuDataMemoryBarrier(void)
{
	__asm__ volatile ("dmb");
}


static inline void hal_cpuDataSyncBarrier(void)
{
	__asm__ volatile ("dsb");
}


static inline void hal_cpuInstrBarrier(void)
{
	__asm__ volatile ("isb");
}

#endif
