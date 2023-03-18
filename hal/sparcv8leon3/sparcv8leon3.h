/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * sparcv8leon3 related routines
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_SPARCV8LEON3_H_
#define _HAL_SPARCV8LEON3_H_


#include <arch/types.h>


static inline void hal_cpuDataStoreBarrier(void)
{
	__asm__ volatile("stbar;");
}


#endif
