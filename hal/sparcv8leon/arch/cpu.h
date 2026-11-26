/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_LEON3_CPU_H_
#define _PH_HAL_LEON3_CPU_H_

#include "include/arch/sparcv8leon/context.h"
#include "include/page.h"

#define SIZE_PAGE _PAGE_SIZE

#ifdef NOMMU

/* Default kernel and user stack sizes */
#ifndef SIZE_KSTACK
#define SIZE_KSTACK (8U * SIZE_PAGE)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (8U * SIZE_PAGE)
#endif

#else

/* Default kernel and user stack sizes */
#ifndef SIZE_KSTACK
#define SIZE_KSTACK (SIZE_PAGE)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (4U * SIZE_PAGE)
#endif

#endif

#define HAL_TLS_VARIANT 2
#define HAL_TLS_TCBSZ   0U
#define HAL_TLS_ARM_M   0

#define CPU_CTX_SIZE 0xd8
#define CPU_EXC_SIZE 0xe0

/* Processor State Register */
#define PSR_CWP 0x1fU        /* Current window pointer */
#define PSR_ET  (1U << 5)    /* Enable traps */
#define PSR_PS  (1U << 6)    /* Previous supervisor */
#define PSR_S   (1U << 7)    /* Supervisor */
#define PSR_PIL (0xfU << 8)  /* Processor interrupt level */
#define PSR_EF  (1U << 12)   /* Enable floating point */
#define PSR_EC  (1U << 13)   /* Enable co-processor */
#define PSR_ICC (0xfU << 20) /* Integer condition codes */


/* Cache control register */
#define CCR_ICS (3U << 0)   /* ICache state */
#define CCR_DCS (3U << 2)   /* DCache state */
#define CCR_IF  (1U << 4)   /* ICache freeze on interrupt */
#define CCR_DF  (1U << 5)   /* DCache freeze on interrupt */
#define CCR_DP  (1UL << 14) /* DCache flush pending */
#define CCR_IP  (1UL << 15) /* ICache flush pending */
#define CCR_IB  (1UL << 16) /* ICache burst fetch en */
#define CCR_FI  (1UL << 21) /* Flush ICache */
#define CCR_FD  (1UL << 22) /* Flush DCache */
#define CCR_DS  (1UL << 23) /* DCache snooping */

/* Basic address space identifiers */
#define ASI_USER_INSTR  0x08
#define ASI_SUPER_INSTR 0x09
#define ASI_USER_DATA   0x0a
#define ASI_SUPER_DATA  0x0b

/* AMBA PnP */
#define AHB_IOAREA 0xfff00000U

/* Interrupts multilock */

/* clang-format off */
#define MULTILOCK_CLEAR \
	stbar; \
	set hal_multilock, %g1; \
	stub %g0, [%g1]

/* FP context save */
#define FPU_SAVE(spOffs) \
	std %f0, [%sp + (spOffs) + 0x00]; \
	std %f2, [%sp + (spOffs) + 0x08]; \
	std %f4, [%sp + (spOffs) + 0x10]; \
	std %f6, [%sp + (spOffs) + 0x18]; \
	std %f8, [%sp + (spOffs) + 0x20]; \
	std %f10, [%sp + (spOffs) + 0x28]; \
	std %f12, [%sp + (spOffs) + 0x30]; \
	std %f14, [%sp + (spOffs) + 0x38]; \
	std %f16, [%sp + (spOffs) + 0x40]; \
	std %f18, [%sp + (spOffs) + 0x48]; \
	std %f20, [%sp + (spOffs) + 0x50]; \
	std %f22, [%sp + (spOffs) + 0x58]; \
	std %f24, [%sp + (spOffs) + 0x60]; \
	std %f26, [%sp + (spOffs) + 0x68]; \
	std %f28, [%sp + (spOffs) + 0x70]; \
	std %f30, [%sp + (spOffs) + 0x78]; \
	st  %fsr, [%sp + (spOffs) + 0x80]; \
	st  %g0, [%sp + (spOffs) + 0x84]


#define FPU_RESTORE(spOffs) \
	ldd [%sp + (spOffs) + 0x00], %f0; \
	ldd [%sp + (spOffs) + 0x08], %f2; \
	ldd [%sp + (spOffs) + 0x10], %f4; \
	ldd [%sp + (spOffs) + 0x18], %f6; \
	ldd [%sp + (spOffs) + 0x20], %f8; \
	ldd [%sp + (spOffs) + 0x28], %f10; \
	ldd [%sp + (spOffs) + 0x30], %f12; \
	ldd [%sp + (spOffs) + 0x38], %f14; \
	ldd [%sp + (spOffs) + 0x40], %f16; \
	ldd [%sp + (spOffs) + 0x48], %f18; \
	ldd [%sp + (spOffs) + 0x50], %f20; \
	ldd [%sp + (spOffs) + 0x58], %f22; \
	ldd [%sp + (spOffs) + 0x60], %f24; \
	ldd [%sp + (spOffs) + 0x68], %f26; \
	ldd [%sp + (spOffs) + 0x70], %f28; \
	ldd [%sp + (spOffs) + 0x78], %f30; \
	ld  [%sp + (spOffs) + 0x80], %fsr

/* clang-format on */

#ifndef __ASSEMBLY__


#include "hal/types.h"
#include "hal/timer.h"
#include "gaisler/gaisler.h"


#define MAX_CPU_COUNT NUM_CPUS

#define SYSTICK_INTERVAL 1000


#define SIZE_STACK_ARG(sz) (((sz) + 3U) & ~0x3U)

/* parasoft-begin-suppress MISRAC2012-RULE_20_7 "t used as type -  wrong interpretation" */
#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		/* 8-byte values might have been put on stack unaligned */ \
		if (sizeof(t) == 8U && ((ptr_t)(ustack) & 0x7U) != 0U) { \
			union { \
				t val; \
				struct { \
					u32 lo; \
					u32 hi; \
				}; \
			} data##n; \
			data##n.lo = *(u32 *)(ustack); \
			data##n.hi = *(u32 *)((ustack) + 4); \
			(v) = data##n.val; \
		} \
		else { \
			(v) = *(t *)(ustack); \
		} \
		(ustack) += SIZE_STACK_ARG(sizeof(t)); \
	} while (0)
/* parasoft-end-suppress MISRAC2012-RULE_20_7*/


/* performance */


static inline void hal_cpuSetDevBusy(int s)
{
}


#ifdef NOMMU
static inline void hal_cpuGetCycles(cycles_t *cb)
{
	*cb = (cycles_t)hal_timerGetUs();
}


/* parasoft-begin-suppress MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static inline void *hal_cpuGetGot(void)
{
	void *got;

	/* clang-format off */
	__asm__ volatile ("mov %%g6, %0" : "=r" (got));
	/* clang-format on */

	return got;
}
/* parasoft-end-suppress MISRAC2012-DIR_4_3 MISRAC2012-RULE_2_1 */
#endif


/* context management */


static inline void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got)
{
	ctx->g6 = (ptr_t)got;
}


static inline void hal_cpuSetGot(void *got)
{
	/* clang-format off */
	__asm__ volatile ("mov %0, %%g6" ::"r" (got));
	/* clang-format on */
}


static inline void hal_cpuSetReturnValue(cpu_context_t *ctx, void *retval)
{
	ctx->o0 = (u32)retval;
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
	return ((ctx->psr & PSR_PS) >> 6 != 0U) ? 1 : 0;
}


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->savesp = (u32)next;
}


/* core management */


static inline unsigned int hal_cpuGetCount(void)
{
	return NUM_CPUS;
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static inline unsigned int hal_cpuGetID(void)
{
	u32 asr17;

	/* clang-format off */
	__asm__ volatile ("rd %%asr17, %0" : "=r" (asr17));
	/* clang-format on */

	return asr17 >> 28;
}


static inline void hal_cpuDisableInterrupts(void)
{
	/* clang-format off */
	__asm__ volatile ("ta 0x09;" ::: "memory");
	/* clang-format on */
}


static inline void hal_cpuEnableInterrupts(void)
{
	/* clang-format off */
	__asm__ volatile ("ta 0x0a;" ::: "memory");
	/* clang-format on */
}


/* parasoft-suppress-next-line MISRAC2012-RULE_2_1-h "function used in target-specific cpuInitCore()" */
static inline void hal_cpuAtomicInc(volatile u32 *dst)
{
	/* clang-format off */
	__asm__ volatile (
		"ld [%0], %%g1\n\t"
	"1: \n\t"
		"mov %%g1, %%g2\n\t"
		"inc %%g1\n\t"
	".align 16\n\t" /* GRLIB TN-0011 errata */
		"casa [%0] %c1, %%g2, %%g1\n\t"
		"cmp %%g1, %%g2\n\t"
		"bne 1b\n\t"
		"nop\n\t"
		:
		: "r"(dst), "i"(ASI_SUPER_DATA)
		: "g1", "g2", "memory"
	);
	/* clang-format on */
}


#endif /* __ASSEMBLY__ */


#endif /*_PH_HAL_LEON3_CPU_H_ */
