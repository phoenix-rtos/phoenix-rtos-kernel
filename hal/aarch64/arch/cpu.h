/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014, 2017, 2018, 2024 Phoenix Systems
 * Author: Jacek Popko, Aleksander Kaminski, Pawel Pisarczyk, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_AARCH64_CPU_H_
#define _HAL_AARCH64_CPU_H_

#include "hal/types.h"
#include "config.h"

#define SIZE_PAGE 0x1000uL
#define SIZE_PDIR SIZE_PAGE

#define SIZE_INITIAL_KSTACK (2 * SIZE_PAGE) /* Must be multiple of page size */

#ifndef SIZE_KSTACK
#define SIZE_KSTACK (2 * SIZE_PAGE)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (8 * SIZE_PAGE)
#endif

#define MODE_nAARCH64 0x10
#define MODE_EL0      0x0
#define MODE_EL1_SP0  0x4
#define MODE_EL1_SP1  0x5
#define MODE_MASK     0xf
#define NO_DBGE       0x200             /* mask to disable debug exception */
#define NO_SERR       0x100             /* mask to disable SError exception */
#define NO_IRQ        0x80              /* mask to disable IRQ */
#define NO_FIQ        0x40              /* mask to disable FIQ */
#define NO_INT        (NO_IRQ | NO_FIQ) /* mask to disable IRQ and FIQ */


#ifndef __ASSEMBLY__

#define SYSTICK_INTERVAL 1000


#define SIZE_STACK_ARG(sz) (((sz) + 7u) & ~0x7u)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		ustack = (void *)(((addr_t)ustack + sizeof(t) - 1) & ~(sizeof(t) - 1)); \
		(v) = *(t *)ustack; \
		ustack += SIZE_STACK_ARG(sizeof(t)); \
	} while (0)

typedef struct _cpu_context_t {
	u64 savesp;
	u64 cpacr;
#ifndef __SOFTFP__
	/* Advanced SIMD/FPU */
	u64 fpcr;
	u64 fpsr;
	u64 freg[2 * 32];
#endif

	u64 psr;
	u64 pc;
	u64 x[31]; /* General purpose registers */
	u64 sp;
} cpu_context_t;


static inline void hal_cpuDisableInterrupts(void)
{
	__asm__ volatile("msr daifSet, #3\n dsb ish \n isb");
}


static inline void hal_cpuEnableInterrupts(void)
{
	__asm__ volatile("msr daifClr, #3\n dsb ish \n isb");
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
	return 63 - __builtin_clzl(v);
}


static inline unsigned int hal_cpuGetFirstBit(unsigned long v)
{

	return __builtin_ctzl(v);
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
	curr->savesp = (u64)next;
}


static inline void hal_cpuSetReturnValue(cpu_context_t *ctx, void *retval)
{
	ctx->x[0] = (u64)retval;
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
	return ctx->psr & MODE_MASK;
}


static inline unsigned int hal_cpuGetID(void)
{
	u64 mpidr;
	/* clang-format off */
	__asm__ volatile ("mrs %0, mpidr_el1" : "=r"(mpidr));
	/* clang-format on */
	return mpidr & 0xff;
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
	__asm__ volatile ("dsb ish\n wfe");
	/* clang-format on */
}


static inline u32 hal_cpuAtomicGet(volatile u32 *dst)
{
	u32 result;
	/* clang-format off */
	__asm__ volatile (
		"ldar %0, [%1]\n"
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
	"1:\n"
		"ldaxr w2, [%0]\n"
		"add w2, w2, #1\n"
		"stlxr w1, w2, [%0]\n"
		"cbnz w1, 1b\n"
		:
		: "r"(dst)
		: "w1", "w2", "memory"
	);
	/* clang-format on */
}


extern unsigned int hal_cpuGetCount(void);


#endif

#endif
