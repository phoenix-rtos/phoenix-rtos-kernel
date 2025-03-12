/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Hardware Abstraction Layer ELF support
 *
 * Copyright 2024 Phoenix Systems
 * Author: Hubert Badocha
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ARMV7R_ELF_H_
#define _HAL_ARMV7R_ELF_H_


#define R_ARM_ABS32   2
#define R_ARM_TARGET1 38


static inline int hal_isRelReloc(int relType)
{
	return ((relType == R_ARM_ABS32) || (relType == R_ARM_TARGET1)) ? 1 : 0;
}

#endif
