/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014, 2017, 2022 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ARMV8M_CPU_H_
#define _HAL_ARMV8M_CPU_H_

#include "hal/types.h"

#define SIZE_PAGE 0x200U

#ifndef SIZE_USTACK
#define SIZE_USTACK (3U * SIZE_PAGE)
#endif

#ifndef SIZE_KSTACK
#define SIZE_KSTACK (4U * SIZE_PAGE)
#endif

/* If KERNEL_FPU_SUPPORT == 0, FPU/MVE context handling in the kernel will be disabled.
 * This flag must be set externally to 1 by the build system to enable FPU handling. */
#ifndef KERNEL_FPU_SUPPORT
#define KERNEL_FPU_SUPPORT 0
#endif

/* values based on EXC_RETURN requirements */
#define EXC_RETURN_SPSEL (1u << 2) /* 1 - was using process SP, 0 - was using main SP */
#define EXC_RETURN_FTYPE (1u << 4) /* 1 - standard frame, 0 - frame with FPU state */

#define DEFAULT_PSR 0x01000000

#if KERNEL_FPU_SUPPORT
#define RET_HANDLER_MSP 0xffffffe1u
#define RET_THREAD_MSP  0xffffffe9u
#define RET_THREAD_PSP  0xffffffedu
#define HWCTXSIZE       (8 + 18)
#define USERCONTROL     0x7u
#else
#define RET_HANDLER_MSP 0xfffffff1u
#define RET_THREAD_MSP  0xfffffff9u
#define RET_THREAD_PSP  0xfffffffdu
#define HWCTXSIZE       8
#define USERCONTROL     0x3u
#endif

#ifndef __ASSEMBLY__

#include "hal/arm/scs.h"
#include "hal/arm/barriers.h"

#define SYSTICK_INTERVAL 1000


#define SIZE_STACK_ARG(sz) (((sz) + 3u) & ~0x3u)


/* parasoft-begin-suppress MISRAC2012-RULE_20_7 "t used as type -  wrong interpretation" */
#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		(ustack) = (void *)(((ptr_t)(ustack) + sizeof(t) - 1U) & ~(sizeof(t) - 1U)); \
		(v) = *(t *)(ustack); \
		(ustack) += SIZE_STACK_ARG(sizeof(t)); \
	} while (0)
/* parasoft-end-suppress MISRAC2012-RULE_20_7*/


typedef struct {
	u32 r0;
	u32 r1;
	u32 r2;
	u32 r3;
	u32 r12;
	u32 lr;
	u32 pc;
	u32 psr;
} cpu_hwContext_t;


typedef struct _cpu_context_t {
	u32 savesp_s;
	u32 fpuctx; /* If KERNEL_FPU_SUPPORT == 0 fpuctx is unused, otherwise it is the value of FPCAR at exception entry. */

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

	u32 msp;
	u32 pad0;

#if KERNEL_FPU_SUPPORT
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
	cpu_hwContext_t hwctx;

#if KERNEL_FPU_SUPPORT
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
	u32 vpr;
#endif
} cpu_context_t;


static inline void hal_cpuDisableInterrupts(void)
{
	__asm__ volatile("cpsid if");
}


static inline void hal_cpuEnableInterrupts(void)
{
	__asm__ volatile("cpsie if");
}


static inline void hal_cpuHalt(void)
{
	__asm__ volatile("dsb \n wfi");
}


/* bit operations */


static inline unsigned int hal_cpuGetLastBit(unsigned long v)
{
	int pos;
	/* clang-format off */
	__asm__ volatile("clz %0, %1" : "=r" (pos) : "r" (v));

	return 31 - pos;
}


static inline unsigned int hal_cpuGetFirstBit(unsigned long v)
{
	unsigned pos;

	__asm__ volatile("\
		rbit %0, %1; \
		clz  %0, %0;" : "=r" (pos) : "r" (v));

	return pos;
}


/* context management */

static inline void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got)
{
	ctx->r9 = (u32)got;
}


static inline void hal_cpuSetGot(void *got)
{
	__asm__ volatile("mov r9, %0" :: "r" (got));
}


static inline void *hal_cpuGetGot(void)
{
	void *got;

	__asm__ volatile("mov %0, r9" : "=r" (got));

	return got;
}


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->savesp_s = (u32)next;
}


static inline void hal_cpuSetReturnValue(cpu_context_t *ctx, void *retval)
{
	ctx->hwctx.r0 = (u32)retval;
}


static inline void _hal_cpuSetKernelStack(void *kstack)
{
}


static inline void *hal_cpuGetSP(cpu_context_t *ctx)
{
	return (void *)ctx;
}


static inline void *hal_cpuGetUserSP(cpu_context_t *ctx)
{
	return (void *)ctx->psp;
}


static inline int hal_cpuSupervisorMode(cpu_context_t *ctx)
{
	return ((ctx->irq_ret & (1 << 2)) == 0) ? 1 : 0;
}


/* core management */


static inline unsigned int hal_cpuGetID(void)
{
	return 0;
}


static inline unsigned int hal_cpuGetCount(void)
{
	return 1;
}


#endif

#endif
