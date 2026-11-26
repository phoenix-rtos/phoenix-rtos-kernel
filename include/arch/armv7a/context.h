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

#ifndef _PH_ARCH_ARMV7A_CONTEXT_H_
#define _PH_ARCH_ARMV7A_CONTEXT_H_


#ifndef __ASSEMBLY__


#include "stdtypes.h"


typedef struct _cpu_context_t {
	__u32 savesp;
	__u32 padding;

	/* Floating point coprocessor context */
	__u32 fpsr;
	__u32 freg[32 * 2];

	__u32 psr;

	__u32 r0;
	__u32 r1;
	__u32 r2;
	__u32 r3;
	__u32 r4;
	__u32 r5;
	__u32 r6;
	__u32 r7;
	__u32 r8;
	__u32 r9;
	__u32 r10;

	__u32 fp;
	__u32 ip;
	__u32 sp;
	__u32 lr;

	__u32 pc;
} cpu_context_t;


#endif


#endif
