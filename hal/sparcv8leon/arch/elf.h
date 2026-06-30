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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_LEON3_ELF_H_
#define _PH_HAL_LEON3_ELF_H_

#define R_SPARC_32 3U


static inline int hal_isRelReloc(unsigned char relType)
{
	return (relType == R_SPARC_32) ? 1 : 0;
}


#endif
