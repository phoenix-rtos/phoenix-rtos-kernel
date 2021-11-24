/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014, 2017 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ARMV7M_CPU_H_
#define _HAL_ARMV7M_CPU_H_


#if defined(CPU_STM32L152XD) || defined(CPU_STM32L152XE) || defined(CPU_STM32L4X6)
#define CPU_STM32
#endif

#if defined(CPU_IMXRT105X) || defined(CPU_IMXRT106X) || defined(CPU_IMXRT117X)
#define CPU_IMXRT
#endif

#include "types.h"

#define SIZE_PAGE 0x200

#ifndef SIZE_USTACK
#define SIZE_USTACK (3 * SIZE_PAGE)
#endif

#ifndef SIZE_KSTACK
#define SIZE_KSTACK (2 * 512)
#endif

#ifdef CPU_IMXRT
#define RET_HANDLER_MSP 0xffffffe1
#define RET_THREAD_MSP  0xffffffe9
#define RET_THREAD_PSP  0xffffffed
#define HWCTXSIZE       (8 + 18)
#define USERCONTROL     0x7
#else
#define RET_HANDLER_MSP 0xfffffff1
#define RET_THREAD_MSP  0xfffffff9
#define RET_THREAD_PSP  0xfffffffd
#define HWCTXSIZE       8
#define USERCONTROL     0x3
#endif

#ifndef __ASSEMBLY__


#define SYSTICK_INTERVAL 1000


#define PUTONSTACK(kstack, t, v) \
	do { \
		(kstack) -= (sizeof(t) + 3) & ~0x3; \
		*((t *)kstack) = (v); \
	} while (0)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		ustack = (void *)(((ptr_t)ustack + sizeof(t) - 1) & ~(sizeof(t) - 1)); \
		(v) = *(t *)ustack; \
		ustack += (sizeof(t) + 3) & ~0x3; \
	} while (0)


typedef struct _cpu_context_t {
	u32 savesp;
	u32 fpuctx;

	/* Saved by ISR */
	u32 psp;
	u32 r4;
	u32 r5;
	u32 r6;
	u32 r7;
	u32 r8;
	u32 r9;
	u32 r10;
	u32 r11;
	u32 irq_ret;

#ifdef CPU_IMXRT
	u32 s16;
	u32 s17;
	u32 s18;
	u32 s19;
	u32 s20;
	u32 s21;
	u32 s22;
	u32 s23;
	u32 s24;
	u32 s25;
	u32 s26;
	u32 s27;
	u32 s28;
	u32 s29;
	u32 s30;
	u32 s31;
#endif

	/* Saved by hardware */
	u32 r0;
	u32 r1;
	u32 r2;
	u32 r3;
	u32 r12;
	u32 lr;
	u32 pc;
	u32 psr;

#ifdef CPU_IMXRT
	u32 s0;
	u32 s1;
	u32 s2;
	u32 s3;
	u32 s4;
	u32 s5;
	u32 s6;
	u32 s7;
	u32 s8;
	u32 s9;
	u32 s10;
	u32 s11;
	u32 s12;
	u32 s13;
	u32 s14;
	u32 s15;
	u32 fpscr;
	u32 pad1;
#endif
} cpu_context_t;


#endif

#endif
