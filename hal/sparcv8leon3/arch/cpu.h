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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_LEON3_CPU_H_
#define _HAL_LEON3_CPU_H_


#ifdef NOMMU

#define SIZE_PAGE 0x200

/* Default kernel and user stack sizes */
#ifndef SIZE_KSTACK
#define SIZE_KSTACK (8 * SIZE_PAGE)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (8 * SIZE_PAGE)
#endif

#else

#define SIZE_PAGE 0x1000

/* Default kernel and user stack sizes */
#ifndef SIZE_KSTACK
#define SIZE_KSTACK (SIZE_PAGE)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (SIZE_PAGE)
#endif

#endif

#define CPU_CTX_SIZE 0xd8
#define CPU_EXC_SIZE 0xe8

/* Processor State Register */
#define PSR_CWP 0x1f        /* Current window pointer */
#define PSR_ET  (1 << 5)    /* Enable traps */
#define PSR_PS  (1 << 6)    /* Previous supervisor */
#define PSR_S   (1 << 7)    /* Supervisor */
#define PSR_PIL (0xf << 8)  /* Processor interrupt level */
#define PSR_EF  (1 << 12)   /* Enable floating point */
#define PSR_EC  (1 << 13)   /* Enable co-processor */
#define PSR_ICC (0xf << 20) /* Integer condition codes */


/* Cache control register */
#define CCR_ICS (3 << 0)  /* ICache state */
#define CCR_DCS (3 << 2)  /* DCache state */
#define CCR_IF  (1 << 4)  /* ICache freeze on interrupt */
#define CCR_DF  (1 << 5)  /* DCache freeze on interrupt */
#define CCR_DP  (1 << 14) /* DCache flush pending */
#define CCR_IP  (1 << 15) /* ICache flush pending */
#define CCR_IB  (1 << 16) /* ICache burst fetch en */
#define CCR_FI  (1 << 21) /* Flush ICache */
#define CCR_FD  (1 << 22) /* Flush DCache */
#define CCR_DS  (1 << 23) /* DCache snooping */

/* Basic address space identifiers */
#define ASI_USER_INSTR  0x08
#define ASI_SUPER_INSTR 0x09
#define ASI_USER_DATA   0x0a
#define ASI_SUPER_DATA  0x0b

/* Interrupts multilock */

/* clang-format off */
#define MULTILOCK_CLEAR \
	stbar; \
	set hal_multilock, %g1; \
	stub %g0, [%g1]

/* FP context save */
#define FPU_SAVE \
	std %f0, [%sp + 0x50]; \
	std %f2, [%sp + 0x58]; \
	std %f4, [%sp + 0x60]; \
	std %f6, [%sp + 0x68]; \
	std %f8, [%sp + 0x70]; \
	std %f10, [%sp + 0x78]; \
	std %f12, [%sp + 0x80]; \
	std %f14, [%sp + 0x88]; \
	std %f16, [%sp + 0x90]; \
	std %f18, [%sp + 0x98]; \
	std %f20, [%sp + 0xa0]; \
	std %f22, [%sp + 0xa8]; \
	std %f24, [%sp + 0xb0]; \
	std %f26, [%sp + 0xb8]; \
	std %f28, [%sp + 0xc0]; \
	std %f30, [%sp + 0xc8]; \
	st  %fsr, [%sp + 0xd0]; \
	st  %g0, [%sp + 0xd4]


#define FPU_RESTORE \
	ldd [%sp + 0x50], %f0; \
	ldd [%sp + 0x58], %f2; \
	ldd [%sp + 0x60], %f4; \
	ldd [%sp + 0x68], %f6; \
	ldd [%sp + 0x70], %f8; \
	ldd [%sp + 0x78], %f10; \
	ldd [%sp + 0x80], %f12; \
	ldd [%sp + 0x88], %f14; \
	ldd [%sp + 0x90], %f16; \
	ldd [%sp + 0x98], %f18; \
	ldd [%sp + 0xa0], %f20; \
	ldd [%sp + 0xa8], %f22; \
	ldd [%sp + 0xb0], %f24; \
	ldd [%sp + 0xb8], %f26; \
	ldd [%sp + 0xc0], %f28; \
	ldd [%sp + 0xc8], %f30; \
	ld  [%sp + 0xd0], %fsr

/* clang-format on */

#ifndef __ASSEMBLY__


#include "types.h"
#include "gaisler/gaisler.h"


#define MAX_CPU_COUNT NUM_CPUS

#define SYSTICK_INTERVAL 1000


#define PUTONSTACK(kstack, t, v) \
	do { \
		(kstack) -= (sizeof(t) + 3) & ~0x3; \
		*((t *)kstack) = (v); \
	} while (0)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		/* 8-byte values might have been put on stack unaligned */ \
		/* clang-format off */ \
		if (sizeof(t) == 8 && ((ptr_t)(ustack) & 0x7) != 0) { \
			/* clang-format on */ \
			union { \
				t val; \
				struct { \
					u32 lo; \
					u32 hi; \
				}; \
			} data##n; \
			data##n.lo = *(u32 *)ustack; \
			data##n.hi = *(u32 *)(ustack + 4); \
			v = data##n.val; \
		} \
		else { \
			(v) = *(t *)ustack; \
		} \
		ustack += (sizeof(t) + 3) & ~0x3; \
	} while (0)


typedef struct {
	u32 f0;
	u32 f1;
	u32 f2;
	u32 f3;
	u32 f4;
	u32 f5;
	u32 f6;
	u32 f7;
	u32 f8;
	u32 f9;
	u32 f10;
	u32 f11;
	u32 f12;
	u32 f13;
	u32 f14;
	u32 f15;
	u32 f16;
	u32 f17;
	u32 f18;
	u32 f19;
	u32 f20;
	u32 f21;
	u32 f22;
	u32 f23;
	u32 f24;
	u32 f25;
	u32 f26;
	u32 f27;
	u32 f28;
	u32 f29;
	u32 f30;
	u32 f31;

	u32 fsr;
	u32 pad;
} cpu_fpContext_t;


typedef struct {
	/* local */
	u32 l0;
	u32 l1;
	u32 l2;
	u32 l3;
	u32 l4;
	u32 l5;
	u32 l6;
	u32 l7;

	/* in */
	u32 i0;
	u32 i1;
	u32 i2;
	u32 i3;
	u32 i4;
	u32 i5;
	u32 fp;
	u32 i7;
} cpu_winContext_t;


typedef struct _cpu_context_t {
	u32 savesp;

	u32 y;
	u32 psr;
	u32 pc;
	u32 npc;

	/* global */
	u32 g1;
	u32 g2;
	u32 g3;
	u32 g4;
	u32 g5;
	u32 g6;
	u32 g7;

	/* out */
	u32 o0;
	u32 o1;
	u32 o2;
	u32 o3;
	u32 o4;
	u32 o5;
	u32 sp;
	u32 o7;

	cpu_fpContext_t fpCtx;
} cpu_context_t;


extern time_t hal_timerGetUs(void);


/* performance */


static inline void hal_cpuSetDevBusy(int s)
{
}


static inline void hal_cpuGetCycles(cycles_t *cb)
{
	*cb = (cycles_t)hal_timerGetUs();
}


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


static inline void *hal_cpuGetGot(void)
{
	void *got;

	/* clang-format off */

	__asm__ volatile ("mov %%g6, %0" : "=r" (got));

	/* clang-format on */

	return got;
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
	return (ctx->psr & PSR_PS) >> 6;
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


#endif /* _HAL_LEON3_CPU_H_ */
