/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines (RISCV64)
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_HAL_RISCV64_CPU_H_
#define _PH_HAL_RISCV64_CPU_H_

#include "hal/types.h"
#include "include/arch/riscv64/context.h"
#include "include/page.h"

#define SIZE_PAGE _PAGE_SIZE

#define MAX_CPU_COUNT 8U

#define SIZE_INITIAL_KSTACK (4U * SIZE_PAGE)
#define INITIAL_KSTACK_BIT  (14)

/* Default kernel and user stack sizes */
#ifndef SIZE_KSTACK
#define SIZE_KSTACK (4U * SIZE_PAGE)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (8U * SIZE_PAGE)
#endif

#define HAL_TLS_VARIANT 1
#define HAL_TLS_TCBSZ   0U
#define HAL_TLS_ARM_M   0

/* Supervisor Cause Register */
#define SCAUSE_INTR (1ULL << 63)

/* Exception codes */
#define SCAUSE_ILLEGAL 2U /* Illegal instruction */
#define SCAUSE_ECALL   8U /* Environment call from S-mode */

/* Supervisor Status Register */
#define SSTATUS_SIE  (1U << 1)   /* Supervisor Interrupt Enable */
#define SSTATUS_SPP  (1UL << 8)  /* Previous Supervisor */
#define SSTATUS_SPIE (1UL << 5)  /* Previous Supervisor IE */
#define SSTATUS_FS   (3UL << 13) /* FPU status */
#define SSTATUS_SUM  (1UL << 18) /* Supervisor may access User Memory */
#define SSTATUS_MXR  (1UL << 19) /* Make eXecutable Readable */

/* Interrupts */
#define CLINT_IRQ_FLG (1UL << 31) /* Marks that interrupt handler is installed for CLINT, not PLIC */

/* Supervisor Interrupt Pending Register */
#define SIP_SSIP (1U << 1)


#define CPU_CTX_SIZE 0x230U


#ifndef __ASSEMBLY__

/* FIXME: This header has to be attached to be used in syscalls.c - ugly ifdef */
#include "hal/riscv64/sbi.h"

#define SYSTICK_INTERVAL 1000


#define SIZE_STACK_ARG(sz) (((sz) + 7U) & ~0x7U)


/* parasoft-begin-suppress MISRAC2012-RULE_20_7 "t used as type -  wrong interpretation" */
#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		(ustack) = (u8 *)(((addr_t)(ustack) + sizeof(t) - 1U) & ~(sizeof(t) - 1U)); \
		(v) = *(t *)(ustack); \
		(ustack) += SIZE_STACK_ARG(sizeof(t)); \
	} while (0)
/* parasoft-end-suppress MISRAC2012-RULE_20_7*/


struct _pmap_t;


/* interrupts */


static inline void hal_cpuDisableInterrupts(void)
{
	__asm__ volatile("csrc sstatus, 2");
}


static inline void hal_cpuEnableInterrupts(void)
{
	__asm__ volatile("csrs sstatus, 2");
}


/* performance */


static inline void hal_cpuHalt(void)
{
	__asm__ volatile("wfi");
}


static inline void hal_cpuSetDevBusy(int s)
{
	(void)s;
}

/* Atomic operations */


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static inline u32 hal_cpuAtomicAdd(volatile u32 *dst, u32 v)
{
	u32 old;
	/* clang-format off */
	__asm__ volatile (
		"fence iorw, ow\n\t"
		"amoadd.w.aq %0, %2, (%1)"
		: "=r"(old), "+r"(dst)
		: "r"(v)
	);
	/* clang-format on */
	return old;
}


/* context management */


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static inline u64 hal_cpuGetGP(void)
{
	u64 gp;
	/* clang-format off */
	__asm__ volatile(
		"mv %0, gp"
		: "=r"(gp)
		:
		:
	);
	/* clang-format on */
	return gp;
}


static inline void hal_cpuSetGP(u64 gp)
{
	/* clang-format off */
	__asm__ volatile(
		"mv gp, %0"
		:
		: "r"(gp)
		:
	);
	/* clang-format on */
}


static inline void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got)
{
	(void)ctx;
	(void)got;
}


static inline void hal_cpuSetGot(void *got)
{
	(void)got;
}


static inline void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->ksp = (u64)next;
}


static inline void hal_cpuSetReturnValue(cpu_context_t *ctx, void *retval)
{
	ctx->a0 = (u64)retval;
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
	return ((ctx->sstatus & SSTATUS_SPP) != 0U) ? 1 : 0;
}


void hal_cpuRfenceI(void);


void hal_cpuLocalFlushTLB(u32 asid, const void *vaddr);


void hal_cpuRemoteFlushTLB(u32 asid, const void *vaddr, size_t size);


#endif

#endif
