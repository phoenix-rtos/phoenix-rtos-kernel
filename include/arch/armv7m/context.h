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

#ifndef _PH_ARCH_ARMV7M_CONTEXT_H_
#define _PH_ARCH_ARMV7M_CONTEXT_H_


#ifndef __ASSEMBLY__


#include "stdtypes.h"


/* Registers saved by hardware on exception entry */
typedef struct {
	__u32 r0;
	__u32 r1;
	__u32 r2;
	__u32 r3;
	__u32 r12;
	__u32 lr;
	__u32 pc;
	__u32 psr;
} cpu_hwContext_t;


typedef struct _cpu_context_t {
	__u32 savesp;
	__u32 fpuctx;

	/* Saved by ISR */
	__u32 psp;
	__u32 r4;
	__u32 r5;
	__u32 r6;
	__u32 r7;
	__u32 r8;
	__u32 r9;
	__u32 r10;
	__u32 r11;
	__u32 irq_ret;

	__u32 msp;
	__u32 pad0;

#if defined(__CPU_IMXRT105X) || defined(__CPU_IMXRT106X) || defined(__CPU_IMXRT117X)
	/* parasoft-suppress-next-line MISRAC2012-RULE_5_6 "s16 is a signed 16-bit typedef and also a register field identifier" */
	__u32 s16;
	__u32 s17;
	__u32 s18;
	__u32 s19;
	__u32 s20;
	__u32 s21;
	__u32 s22;
	__u32 s23;
	__u32 s24;
	__u32 s25;
	__u32 s26;
	__u32 s27;
	__u32 s28;
	__u32 s29;
	__u32 s30;
	__u32 s31;
#endif

	/* Saved by hardware */
	cpu_hwContext_t hwctx;

#if defined(__CPU_IMXRT105X) || defined(__CPU_IMXRT106X) || defined(__CPU_IMXRT117X)
	__u32 s0;
	__u32 s1;
	__u32 s2;
	__u32 s3;
	__u32 s4;
	__u32 s5;
	__u32 s6;
	__u32 s7;
	/* parasoft-suppress-next-line MISRAC2012-RULE_5_6 "s8 is a signed 8-bit typedef and also a register field identifier" */
	__u32 s8;
	__u32 s9;
	__u32 s10;
	__u32 s11;
	__u32 s12;
	__u32 s13;
	__u32 s14;
	__u32 s15;
	__u32 fpscr;
	__u32 pad1;
#endif
} cpu_context_t;


#endif


#endif
