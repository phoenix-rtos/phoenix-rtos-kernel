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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_ARMV8M_CPU_H_
#define _PH_HAL_ARMV8M_CPU_H_

#include "hal/types.h"
#include "include/arch/armv8m/context.h"
#include "include/page.h"

#define SIZE_PAGE _PAGE_SIZE

#ifndef SIZE_USTACK
#define SIZE_USTACK (3U * SIZE_PAGE)
#endif

#define HAL_TLS_VARIANT 1
#define HAL_TLS_TCBSZ   8U
#define HAL_TLS_ARM_M   1

#ifndef SIZE_KSTACK
#define SIZE_KSTACK (4U * SIZE_PAGE)
#endif

/* If KERNEL_FPU_SUPPORT == 0, FPU/MVE context handling in the kernel will be disabled.
 * This flag must be set externally to 1 by the build system to enable FPU handling. */
#ifndef KERNEL_FPU_SUPPORT
#define KERNEL_FPU_SUPPORT 0
#endif

/* values based on EXC_RETURN requirements */
#define EXC_RETURN_SPSEL (1U << 2) /* 1 - was using process SP, 0 - was using main SP */
#define EXC_RETURN_FTYPE (1U << 4) /* 1 - standard frame, 0 - frame with FPU state */

#define DEFAULT_PSR 0x01000000U

#if KERNEL_FPU_SUPPORT
#define RET_HANDLER_MSP 0xffffffe1U
#define RET_THREAD_MSP  0xffffffe9U
#define RET_THREAD_PSP  0xffffffedU
#define HWCTXSIZE       (8U + 18U)
#define USERCONTROL     0x7U
#else
#define RET_HANDLER_MSP 0xfffffff1U
#define RET_THREAD_MSP  0xfffffff9U
#define RET_THREAD_PSP  0xfffffffdU
#define HWCTXSIZE       8U
#define USERCONTROL     0x3U
#endif

#define KERNEL_SPINLOCK_ARM_M 1

#ifndef __ASSEMBLY__

#include "hal/arm/scs.h"
#include "hal/arm/barriers.h"

#define SYSTICK_INTERVAL 1000


#define SIZE_STACK_ARG(sz) (((sz) + 3U) & ~0x3U)


/* parasoft-begin-suppress MISRAC2012-RULE_20_7 "t used as type -  wrong interpretation" */
#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		(ustack) = (u8 *)(((ptr_t)(ustack) + sizeof(t) - 1U) & ~(sizeof(t) - 1U)); \
		(v) = *(t *)(ustack); \
		(ustack) += SIZE_STACK_ARG(sizeof(t)); \
	} while (0)
/* parasoft-end-suppress MISRAC2012-RULE_20_7*/


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


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static inline unsigned int hal_cpuGetLastBit(unsigned long v)
{
	unsigned int pos;
	/* clang-format off */
	__asm__ volatile("clz %0, %1" : "=r" (pos) : "r" (v));

	return 31U - pos;
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static inline unsigned int hal_cpuGetFirstBit(unsigned long v)
{
	unsigned int pos;

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


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
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
	return ((ctx->irq_ret & (1U << 2)) == 0U) ? 1 : 0;
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
