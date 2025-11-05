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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_RISCV64_CPU_H_
#define _HAL_RISCV64_CPU_H_

#include "hal/types.h"

#define SIZE_PAGE 0x1000UL

#define MAX_CPU_COUNT 8

#define SIZE_INITIAL_KSTACK (4U * SIZE_PAGE)
#define INITIAL_KSTACK_BIT  (14)

/* Default kernel and user stack sizes */
#ifndef SIZE_KSTACK
#define SIZE_KSTACK (4U * SIZE_PAGE)
#endif

#ifndef SIZE_USTACK
#define SIZE_USTACK (8U * SIZE_PAGE)
#endif

/* Supervisor Cause Register */
#define SCAUSE_INTR (1u << 63)

/* Exception codes */
#define SCAUSE_ILLEGAL 2u /* Illegal instruction */
#define SCAUSE_ECALL   8u /* Environment call from S-mode */

/* Supervisor Status Register */
#define SSTATUS_SIE  (1U << 1)  /* Supervisor Interrupt Enable */
#define SSTATUS_SPP  (1U << 8)  /* Previous Supervisor */
#define SSTATUS_SPIE (1U << 5)  /* Previous Supervisor IE */
#define SSTATUS_FS   (3U << 13) /* FPU status */
#define SSTATUS_SUM  (1U << 18) /* Supervisor may access User Memory */
#define SSTATUS_MXR  (1U << 19) /* Make eXecutable Readable */

/* Interrupts */
#define CLINT_IRQ_FLG (1u << 31) /* Marks that interrupt handler is installed for CLINT, not PLIC */

/* Supervisor Interrupt Pending Register */
#define SIP_SSIP (1U << 1)


#define CPU_CTX_SIZE 0x230U


#ifndef __ASSEMBLY__

/* FIXME: This header has to be attached to be used in syscalls.c - ugly ifdef */
#include "hal/riscv64/sbi.h"

#define SYSTICK_INTERVAL 1000


#define SIZE_STACK_ARG(sz) (((sz) + 7u) & ~0x7U)


#define GETFROMSTACK(ustack, t, v, n) \
	do { \
		ustack = (u8 *)(((addr_t)ustack + sizeof(t) - 1) & ~(sizeof(t) - 1)); \
		(v) = *(t *)ustack; \
		ustack += SIZE_STACK_ARG(sizeof(t)); \
	} while (0)


typedef struct {
	u64 ft0;
	u64 ft1;
	u64 ft2;
	u64 ft3;
	u64 ft4;
	u64 ft5;
	u64 ft6;
	u64 ft7;

	u64 fs0;
	u64 fs1;

	u64 fa0;
	u64 fa1;
	u64 fa2;
	u64 fa3;
	u64 fa4;
	u64 fa5;
	u64 fa6;
	u64 fa7;

	u64 fs2;
	u64 fs3;
	u64 fs4;
	u64 fs5;
	u64 fs6;
	u64 fs7;
	u64 fs8;
	u64 fs9;
	u64 fs10;
	u64 fs11;

	u64 ft8;
	u64 ft9;
	u64 ft10;
	u64 ft11;

	u64 fcsr;
} __attribute__((packed, aligned(8))) cpu_fpContext_t;


/* CPU context saved by interrupt handlers on thread kernel stack */
typedef struct _cpu_context_t {
	u64 ra; /* x1 */
	u64 gp; /* x3 */

	u64 t0; /* x5 */
	u64 t1; /* x6 */
	u64 t2; /* x7 */

	u64 s0; /* x8 */
	u64 s1; /* x9 */
	u64 a0; /* x10 */
	u64 a1; /* x11 */

	u64 a2; /* x12 */
	u64 a3; /* x13 */
	u64 a4; /* x14 */
	u64 a5; /* x15 */

	u64 a6; /* x16 */
	u64 a7; /* x17 */
	u64 s2; /* x18 */
	u64 s3; /* x19 */

	u64 s4; /* x20 */
	u64 s5; /* x21 */
	u64 s6; /* x22 */
	u64 s7; /* x23 */

	u64 s8;  /* x24 */
	u64 s9;  /* x25 */
	u64 s10; /* x26 */
	u64 s11; /* x27 */

	u64 t3; /* x28 */
	u64 t4; /* x29 */
	u64 t5; /* x30 */
	u64 t6; /* x31 */

	u64 ksp;
	u64 sstatus;
	u64 sepc;
	u64 stval;
	u64 scause;
	u64 sscratch;

	u64 tp;
	u64 sp;

	cpu_fpContext_t fpCtx;
} __attribute__((packed, aligned(8))) cpu_context_t;


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


static inline void hal_cpuGetCycles(cycles_t *cb)
{
	/* clang-format off */
	__asm__ volatile("rdcycle %0" : "=r"(*(cycles_t *)cb));
	/* clang-format on */
}


/* Atomic operations */


static inline void hal_cpuAtomicAdd(volatile u32 *dst, u32 v)
{
	/* clang-format off */
	__asm__ volatile (
		"fence iorw, ow\n\t"
		"amoadd.w.aq zero, %1, (%0)"
		: "+r"(dst)
		: "r"(v)
	);
	/* clang-format on */
}


/* context management */


static inline void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got)
{
	(void)ctx;
	(void)got;
}


static inline void hal_cpuSetGot(void *got)
{
	(void)got;
}


static inline void *hal_cpuGetGot(void)
{
	return NULL;
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
	return ((ctx->sstatus & SSTATUS_SPP) != 0) ? 1 : 0;
}


void hal_cpuRfenceI(void);


void hal_cpuLocalFlushTLB(u32 asid, const void *vaddr);


void hal_cpuRemoteFlushTLB(u32 asid, const void *vaddr, size_t size);


#endif

#endif
