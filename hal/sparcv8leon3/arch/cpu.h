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


#ifndef __ASSEMBLY__


#include "types.h"
#include "gaisler/gaisler.h"


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


#endif /* __ASSEMBLY__ */


#endif /* _HAL_LEON3_CPU_H_ */
