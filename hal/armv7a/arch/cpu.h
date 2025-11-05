/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014, 2017, 2018 Phoenix Systems
 * Author: Jacek Popko, Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_ARMV7A_CPU_H_
#define _PH_HAL_ARMV7A_CPU_H_

#include "hal/types.h"
#include "config.h"

#define SIZE_PAGE 0x1000U
#define SIZE_PDIR 0x2000U

#ifndef SIZE_KSTACK
#define SIZE_KSTACK (8U * 1024U)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (8U * SIZE_PAGE)
#endif

#define USR_MODE    0x10
#define FIQ_MODE    0x11
#define IRQ_MODE    0x12
#define SVC_MODE    0x13 /* reset mode */
#define ABT_MODE    0x17
#define UND_MODE    0x1B
#define SYS_MODE    0x1F
#define MODE_MASK   0x1F
#define NO_ABORT    0x100             /* mask to disable Abort Exception */
#define NO_IRQ      0x80              /* mask to disable IRQ */
#define NO_FIQ      0x40              /* mask to disable FIQ */
#define NO_INT      (NO_IRQ | NO_FIQ) /* mask to disable IRQ and FIQ */
#define THUMB_STATE 0x20


#ifndef __ASSEMBLY__

#define SYSTICK_INTERVAL 1000


#define SIZE_STACK_ARG(sz) (((sz) + 3u) & ~0x3u)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		ustack = (u8 *)(((ptr_t)ustack + sizeof(t) - 1) & ~(sizeof(t) - 1)); \
		(v) = *(t *)ustack; \
		ustack += SIZE_STACK_ARG(sizeof(t)); \
	} while (0)

typedef struct _cpu_context_t {
	u32 savesp;
	u32 padding;

	/* Floating point coprocessor context */
	u32 fpsr;
	u32 freg[32 * 2];

	u32 psr;

	u32 r0;
	u32 r1;
	u32 r2;
	u32 r3;
	u32 r4;
	u32 r5;
	u32 r6;
	u32 r7;
	u32 r8;
	u32 r9;
	u32 r10;

	u32 fp;
	u32 ip;
	u32 sp;
	u32 lr;

	u32 pc;
} cpu_context_t;


static inline void hal_cpuDisableInterrupts(void)
{
	__asm__ volatile("cpsid if");
}


static inline void hal_cpuEnableInterrupts(void)
{
	__asm__ volatile("cpsie aif");
}


static inline void hal_cpuHalt(void)
{
	__asm__ volatile("wfi");
}


static inline void hal_cpuSetDevBusy(int s)
{
}


static inline unsigned int hal_cpuGetLastBit(unsigned long v)
{
	int pos;

	__asm__ volatile("clz %0, %1" : "=r"(pos) : "r"(v));

	return 31 - pos;
}


static inline unsigned int hal_cpuGetFirstBit(unsigned long v)
{
	unsigned int pos;

	__asm__ volatile("\
		rbit %0, %1; \
		clz  %0, %0;" : "=r"(pos) : "r"(v));

	return pos;
}


static inline void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got)
{
}


static inline void hal_cpuSetGot(void *got)
{
}


static inline void *hal_cpuGetGot(void)
{
	return NULL;
}


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->savesp = (u32)next /*+ sizeof(u32)*/;
}


static inline void hal_cpuSetReturnValue(cpu_context_t *ctx, void *retval)
{
	ctx->r0 = (u32)retval;
}


static inline void *hal_cpuGetSP(cpu_context_t *ctx)
{
	return (void *)ctx;
}


static inline void *hal_cpuGetUserSP(cpu_context_t *ctx)
{
	return (void *)ctx->sp;
}


static inline int hal_cpuSupervisorMode(cpu_context_t *ctx)
{
	return ctx->psr & 0xf;
}


static inline unsigned int hal_cpuGetID(void)
{
	unsigned int mpidr;
	/* clang-format off */
	__asm__ volatile ("mrc p15, 0, %0, c0, c0, 5": "=r"(mpidr));
	/* clang-format on */
	return mpidr & 0xf;
}


static inline void hal_cpuSignalEvent(void)
{
	/* clang-format off */
	__asm__ volatile ("sev");
	/* clang-format on */
}


static inline void hal_cpuWaitForEvent(void)
{
	/* clang-format off */
	__asm__ volatile ("dsb\n wfe");
	/* clang-format on */
}


static inline u32 hal_cpuAtomicGet(volatile u32 *dst)
{
	u32 result;
	/* clang-format off */
	__asm__ volatile (
		"dmb\n"
		"ldr %0, [%1]\n"
		"dmb\n"
		: "=r"(result)
		: "r"(dst)
	);
	/* clang-format on */
	return result;
}


static inline void hal_cpuAtomicInc(volatile u32 *dst)
{
	/* clang-format off */
	__asm__ volatile (
		"dmb\n"
	"1:\n"
		"ldrex r2, [%0]\n"
		"add r2, r2, #1\n"
		"strex r1, r2, [%0]\n"
		"cmp r1, #0\n"
		"bne 1b\n"
		"dmb\n"
		:
		: "r"(dst)
		: "r1", "r2", "memory"
	);
	/* clang-format on */
}


unsigned int hal_cpuGetCount(void);


#endif

#endif
