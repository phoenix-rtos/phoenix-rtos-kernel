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

#ifndef _PH_HAL_ARMV7R_ELF_H_
#define _PH_HAL_ARMV7R_ELF_H_


#define R_ARM_ABS32   2U
#define R_ARM_TARGET1 38U


static inline int hal_isRelReloc(unsigned char relType)
{
	return ((relType == R_ARM_ABS32) || (relType == R_ARM_TARGET1)) ? 1 : 0;
}

#endif
