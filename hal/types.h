/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL - Basic types
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_TYPES_H_
#define _HAL_TYPES_H_


#define NULL 0


#ifndef __ASSEMBLY__


#include "include/types.h"
#include <arch/types.h>

typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;
typedef __u64 u64;

typedef __s8 s8;
typedef __s16 s16;
typedef __s32 s32;
typedef __s64 s64;


#endif


#endif
