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

#ifndef _PH_ARCH_AARCH64_CONTEXT_H_
#define _PH_ARCH_AARCH64_CONTEXT_H_


#ifndef __ASSEMBLY__


#include "stdtypes.h"


typedef struct _cpu_context_t {
	__u64 savesp;
	__u64 cpacr;
#ifndef __SOFTFP__
	/* Advanced SIMD/FPU */
	__u64 fpcr;
	__u64 fpsr;
	__u64 freg[2 * 32];
#endif

	__u64 psr;
	__u64 pc;
	__u64 x[31]; /* General purpose registers */
	__u64 sp;
} cpu_context_t;


#endif


#endif
