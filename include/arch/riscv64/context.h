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

#ifndef _PH_ARCH_RISCV64_CONTEXT_H_
#define _PH_ARCH_RISCV64_CONTEXT_H_


#ifndef __ASSEMBLY__


#include "stdtypes.h"


typedef struct {
	__u64 ft0;
	__u64 ft1;
	__u64 ft2;
	__u64 ft3;
	__u64 ft4;
	__u64 ft5;
	__u64 ft6;
	__u64 ft7;

	__u64 fs0;
	__u64 fs1;

	__u64 fa0;
	__u64 fa1;
	__u64 fa2;
	__u64 fa3;
	__u64 fa4;
	__u64 fa5;
	__u64 fa6;
	__u64 fa7;

	__u64 fs2;
	__u64 fs3;
	__u64 fs4;
	__u64 fs5;
	__u64 fs6;
	__u64 fs7;
	__u64 fs8;
	__u64 fs9;
	__u64 fs10;
	__u64 fs11;

	__u64 ft8;
	__u64 ft9;
	__u64 ft10;
	__u64 ft11;

	__u64 fcsr;
} __attribute__((packed, aligned(8))) cpu_fpContext_t;


/* CPU context saved by interrupt handlers on thread kernel stack */
typedef struct _cpu_context_t {
	__u64 ra; /* x1 */
	__u64 gp; /* x3 */

	__u64 t0; /* x5 */
	__u64 t1; /* x6 */
	__u64 t2; /* x7 */

	__u64 s0; /* x8 */
	__u64 s1; /* x9 */
	__u64 a0; /* x10 */
	__u64 a1; /* x11 */

	__u64 a2; /* x12 */
	__u64 a3; /* x13 */
	__u64 a4; /* x14 */
	__u64 a5; /* x15 */

	__u64 a6; /* x16 */
	__u64 a7; /* x17 */
	__u64 s2; /* x18 */
	__u64 s3; /* x19 */

	__u64 s4; /* x20 */
	__u64 s5; /* x21 */
	__u64 s6; /* x22 */
	__u64 s7; /* x23 */

	/* parasoft-suppress-next-line MISRAC2012-RULE_5_6 "s8 is a signed 8-bit typedef and also a RISCV64 register field identifier" */
	__u64 s8;  /* x24 */
	__u64 s9;  /* x25 */
	__u64 s10; /* x26 */
	__u64 s11; /* x27 */

	__u64 t3; /* x28 */
	__u64 t4; /* x29 */
	__u64 t5; /* x30 */
	__u64 t6; /* x31 */

	__u64 ksp;
	__u64 sstatus;
	__u64 sepc;
	__u64 stval;
	__u64 scause;
	__u64 sscratch;

	__u64 tp;
	__u64 sp;

	cpu_fpContext_t fpCtx;
} __attribute__((packed, aligned(8))) cpu_context_t;


#endif


#endif
