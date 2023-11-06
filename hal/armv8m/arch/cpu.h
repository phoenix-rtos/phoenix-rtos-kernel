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


#if defined(__CPU_NRF9160)
#define CPU_NRF91
#endif

#include "types.h"

#define SIZE_PAGE 0x200

#ifndef SIZE_USTACK
#define SIZE_USTACK (3 * SIZE_PAGE)
#endif

#ifndef SIZE_KSTACK
#define SIZE_KSTACK (4 * SIZE_PAGE)
#endif

/* values based on EXC_RETURN requirements */
#define RET_HANDLER_MSP 0xfffffff1u
#define RET_THREAD_MSP  0xfffffff9u
#define RET_THREAD_PSP  0xfffffffdu
#define HWCTXSIZE       8
#define USERCONTROL     0x3u

#ifndef __ASSEMBLY__


#define SYSTICK_INTERVAL 1000


#define PUTONSTACK(kstack, t, v) \
	do { \
		(kstack) -= (sizeof(t) + 3) & ~0x3u; \
		*((t *)kstack) = (v); \
	} while (0)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		ustack = (void *)(((ptr_t)ustack + sizeof(t) - 1) & ~(sizeof(t) - 1)); \
		(v) = *(t *)ustack; \
		ustack += (sizeof(t) + 3) & ~0x3u; \
	} while (0)


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
	u32 padding;

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

	/* Saved by hardware */
	cpu_hwContext_t hwctx;

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
	__asm__ volatile("\
		wfi; \
		nop; ");
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
