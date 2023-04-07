/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_LEON3_EXCEPTIONS_H_
#define _HAL_LEON3_EXCEPTIONS_H_

#include "types.h"

#define EXC_DEFAULT 128

#define EXC_UNDEFINED 1

#define SIZE_CTXDUMP 512

#pragma pack(push, 1)

typedef struct _exc_context_t {
	/* global */
	u32 g0;
	u32 g1;
	u32 g2;
	u32 g3;
	u32 g4;
	u32 g5;
	u32 g6;
	u32 g7;

	/* out */
	u32 o0;
	u32 o1;
	u32 o2;
	u32 o3;
	u32 o4;
	u32 o5;
	u32 sp;
	u32 o7;

	/* local */
	u32 l0;
	u32 l1;
	u32 l2;
	u32 l3;
	u32 l4;
	u32 l5;
	u32 l6;
	u32 l7;

	/* in */
	u32 i0;
	u32 i1;
	u32 i2;
	u32 i3;
	u32 i4;
	u32 i5;
	u32 fp;
	u32 i7;

	/* special */
	u32 y;
	u32 psr;
	u32 wim;
	u32 tbr;
	u32 pc;
	u32 npc;
} exc_context_t;

#pragma pack(pop)


#endif
