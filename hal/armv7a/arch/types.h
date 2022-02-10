/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Types
 *
 * Copyright 2014, 2017, 2018 Phoenix Systems
 * Author: Jacek Popko, Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ARMV7A_TYPES_H_
#define _HAL_ARMV7A_TYPES_H_

#define NULL 0

#ifndef __ASSEMBLY__

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;

typedef u32 addr_t;
typedef u32 cycles_t;

typedef u64 usec_t;
typedef s64 offs_t;

typedef unsigned int size_t;
typedef unsigned long long time_t;

typedef u32 ptr_t;

typedef u64 id_t;

/* Object identifier - contains server port and object id */
typedef struct _oid_t {
	u32 port;
	id_t id;
} oid_t;

#endif

#endif
