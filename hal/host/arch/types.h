/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2001, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_HOST_TYPES_H_
#define _HAL_HOST_TYPES_H_

#define NULL 0

#ifndef __ASSEMBLY__

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

typedef u8 ld80[10]; /* Long double as bytes */
typedef struct {
	u16 _controlWord, controlWord;
	u16 _statusWord, statusWord;
	u16 _tagWord, tagWord;
	u32 fip;
	u32 fips;
	u32 fdp;
	u16 _fds, fds;
	ld80 fpuContext[8];
} fpu_context_t;

typedef char s8;
typedef short s16;
typedef int s32;
typedef long long s64;

typedef u32 addr_t;
typedef u64 cycles_t;

typedef u64 usec_t;
/* FIXME: offs_t should be eradicated */
typedef s64 offs_t;
typedef offs_t off_t;

typedef long unsigned int size_t;
typedef unsigned long long time_t;

typedef u32 ptr_t;

/* Object identifier - contains server port and object id */
typedef u64 id_t;
typedef struct _oid_t {
	u32 port;
	id_t id;
} oid_t;

#endif

#endif
