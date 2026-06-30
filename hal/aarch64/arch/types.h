/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU-specific types
 *
 * Copyright 2024 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_AARCH64_TYPES_H_
#define _PH_HAL_AARCH64_TYPES_H_


#include "include/types.h"


#ifndef __ASSEMBLY__


typedef __u64 cycles_t;
typedef __u64 ptr_t;
typedef __u16 asid_t;

typedef unsigned long int size_t;


#endif


#endif
