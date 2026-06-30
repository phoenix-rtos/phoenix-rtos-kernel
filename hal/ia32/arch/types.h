/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2012, 2017, 2024 Phoenix Systems
 * Copyright 2001, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_IA32_TYPES_H_
#define _PH_HAL_IA32_TYPES_H_


#include "include/types.h"


#ifndef __ASSEMBLY__


typedef __u8 ld80[10]; /* Long double as bytes */

typedef __u64 cycles_t;
typedef __u32 ptr_t;

typedef unsigned long size_t;


#endif


#endif
