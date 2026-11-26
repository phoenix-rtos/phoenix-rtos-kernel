/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU context struct
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Klimek
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_ARCH_SPARCV8LEON_CONTEXT_H_
#define _PH_ARCH_SPARCV8LEON_CONTEXT_H_


#ifndef __ASSEMBLY__


#include "stdtypes.h"


typedef struct {
	__u32 f0;
	__u32 f1;
	__u32 f2;
	__u32 f3;
	__u32 f4;
	__u32 f5;
	__u32 f6;
	__u32 f7;
	__u32 f8;
	__u32 f9;
	__u32 f10;
	__u32 f11;
	__u32 f12;
	__u32 f13;
	__u32 f14;
	__u32 f15;
	__u32 f16;
	__u32 f17;
	__u32 f18;
	__u32 f19;
	__u32 f20;
	__u32 f21;
	__u32 f22;
	__u32 f23;
	__u32 f24;
	__u32 f25;
	__u32 f26;
	__u32 f27;
	__u32 f28;
	__u32 f29;
	__u32 f30;
	__u32 f31;

	__u32 fsr;
	__u32 pad;
} cpu_fpContext_t;


/* Register window, spilled to the stack pointed by its sp */
typedef struct {
	/* local */
	__u32 l0;
	__u32 l1;
	__u32 l2;
	__u32 l3;
	__u32 l4;
	__u32 l5;
	__u32 l6;
	__u32 l7;

	/* in */
	__u32 i0;
	__u32 i1;
	__u32 i2;
	__u32 i3;
	__u32 i4;
	__u32 i5;
	__u32 fp;
	__u32 i7;
} cpu_winContext_t;


typedef struct _cpu_context_t {
	__u32 savesp;

	__u32 y;
	__u32 psr;
	__u32 pc;
	__u32 npc;

	/* global */
	__u32 g1;
	__u32 g2;
	__u32 g3;
	__u32 g4;
	__u32 g5;
	__u32 g6;
	__u32 g7;

	/* out */
	__u32 o0;
	__u32 o1;
	__u32 o2;
	__u32 o3;
	__u32 o4;
	__u32 o5;
	__u32 sp;
	__u32 o7;

	cpu_fpContext_t fpCtx;
} cpu_context_t;


#endif


#endif
