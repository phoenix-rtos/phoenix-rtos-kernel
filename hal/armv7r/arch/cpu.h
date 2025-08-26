/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014-2025 Phoenix Systems
 * Author: Jacek Popko, Aleksander Kaminski, Pawel Pisarczyk, Lukasz Leczkowski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_ARMV7R_CPU_H_
#define _HAL_ARMV7R_CPU_H_

#define SIZE_PAGE 0x1000U

#define SIZE_INITIAL_KSTACK  SIZE_PAGE
#define INITIAL_KSTACK_SHIFT 12

#ifndef SIZE_KSTACK
#define SIZE_KSTACK (8U * 1024U)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (8U * SIZE_PAGE)
#endif

/* ARMv7 processor modes */
#define MODE_USR 0x10 /* unprivileged mode in which most applications run                           */
#define MODE_FIQ 0x11 /* entered on an FIQ interrupt exception                                      */
#define MODE_IRQ 0x12 /* entered on an IRQ interrupt exception                                      */
#define MODE_SVC 0x13 /* entered on reset or when a Supervisor Call instruction ( SVC ) is executed */
#define MODE_MON 0x16 /* security extensions                                                        */
#define MODE_ABT 0x17 /* entered on a memory access exception                                       */
#define MODE_HYP 0x1a /* virtualization extensions                                                  */
#define MODE_UND 0x1b /* entered when an undefined instruction executed                             */
#define MODE_SYS 0x1f /* privileged mode, sharing the register view with User mode                  */

#define MODE_MASK   0x1f
#define NO_ABORT    0x100             /* mask to disable Abort Exception */
#define NO_IRQ      0x80              /* mask to disable IRQ             */
#define NO_FIQ      0x40              /* mask to disable FIQ             */
#define NO_INT      (NO_IRQ | NO_FIQ) /* mask to disable IRQ and FIQ     */
#define THUMB_STATE 0x20U


#ifndef __ASSEMBLY__

#include "hal/types.h"


#define SYSTICK_INTERVAL 1000


#define SIZE_STACK_ARG(sz) (((sz) + 3u) & ~0x3u)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		ustack = (void *)(((addr_t)ustack + sizeof(t) - 1U) & ~(sizeof(t) - 1U)); \
		(v) = *(t *)ustack; \
		ustack += SIZE_STACK_ARG(sizeof(t)); \
	} while (0)

typedef struct _cpu_context_t {
	u32 savesp;
	u32 padding;

	/* FPU context */
	u32 fpsr;
	u32 freg[16 * 2];

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

	/* clang-format off */
	__asm__ volatile ("clz %0, %1" : "=r" (pos) : "r" (v));
	/* clang-format on */

	return 31 - pos;
}


static inline unsigned int hal_cpuGetFirstBit(unsigned long v)
{
	unsigned pos;

	/* clang-format off */
	__asm__ volatile (
		"rbit %0, %1\n\t"
		"clz  %0, %0"
		: "=r" (pos) : "r" (v)
	);
	/* clang-format on */

	return pos;
}


static inline void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got)
{
	ctx->r9 = (u32)got;
}


static inline void hal_cpuSetGot(void *got)
{
	/* clang-format off */
	__asm__ volatile ("mov r9, %0" ::"r"(got));
	/* clang-format on */
}


static inline void *hal_cpuGetGot(void)
{
	void *got;

	/* clang-format off */
	__asm__ volatile ("mov %0, r9" : "=r"(got));
	/* clang-format on */

	return got;
}


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->savesp = (u32)next;
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
	return ctx->psr & 0xfU;
}


static inline unsigned int hal_cpuGetID(void)
{
	unsigned mpidr;
	/* clang-format off */
	__asm__ volatile ("mrc p15, 0, %0, c0, c0, 5": "=r"(mpidr));
	/* clang-format on */
	return mpidr & 0xfU;
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
	__asm__ volatile (
		"dsb\n\t"
		"wfe"
	);
	/* clang-format on */
}


static inline u32 hal_cpuAtomicGet(volatile u32 *dst)
{
	u32 result;
	/* clang-format off */
	__asm__ volatile (
		"dmb\n\t"
		"ldr %0, [%1]\n\t"
		"dmb"
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
	"1:\n\t"
		"ldrex r2, [%0]\n\t"
		"add r2, r2, #1\n\t"
		"strex r1, r2, [%0]\n\t"
		"cmp r1, #0\n\t"
		"bne 1b\n\t"
		"dmb"
		:
		: "r"(dst)
		: "r1", "r2", "memory"
	);
	/* clang-format on */
}


static inline void hal_cpuSmpSync(void)
{
}


extern unsigned int hal_cpuGetCount(void);


#endif


#endif
