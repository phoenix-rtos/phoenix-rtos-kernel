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

#ifndef _PH_HAL_TYPES_H_
#define _PH_HAL_TYPES_H_


#define NULL ((void *)0)


#ifndef __ASSEMBLY__


#include "include/types.h"
#include <arch/types.h>

typedef __u8 u8;
typedef __u16 u16;
typedef __u32 u32;
typedef __u64 u64;

/* parasoft-suppress-next-line MISRAC2012-RULE_5_6 "s8 is a signed 8-bit typedef and could also be a register field identifier" */
typedef __s8 s8;
/* parasoft-suppress-next-line MISRAC2012-RULE_5_6 "s8 is a signed 8-bit typedef and could also be a register field identifier" */
typedef __s16 s16;
typedef __s32 s32;
typedef __s64 s64;

typedef void (*startFn_t)(void *arg);

struct _cpu_context_t;

typedef int (*intrFn_t)(unsigned int n, struct _cpu_context_t *ctx, void *arg);

#endif


#endif
