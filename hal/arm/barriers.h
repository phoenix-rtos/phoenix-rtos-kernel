/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * ARM barriers
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_ARM_BARRIERS_H_
#define _PH_HAL_ARM_BARRIERS_H_


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
