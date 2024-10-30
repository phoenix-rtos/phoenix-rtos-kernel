/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level exception (and interrupt) handling for AArch64
 *
 * Copyright 2024 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

#define CPACR_FPU_TRAP_BIT0 20
#define CPACR_FPU_TRAP_BIT1 21

/* Macros for registers that are assumed to have a certain value */
#define X_SPSR_EL1 x3
#define X_ESR_EL1  x28
#define W_ESR_EL1  w28
#define X_SP_EL0   x29

.extern schedulerLocked

.macro unlock_scheduler
	ldr x0, =schedulerLocked
	stlr wzr, [x0] /* Global monitor clear generates an event, SEV not necessary */
.endm

/* This macro creates an exception vector, so it cannot contain more than 32 instructions */
.macro exception_vector fromUserspace doCheckFPUTrap dispatchFunction
	stp x28, x29, [sp, #-0x10]
	mrs X_SP_EL0, sp_el0
.if \fromUserspace
	mrs x28, tpidr_el1
.else
	mov x28, X_SP_EL0
.endif
	stp x30, X_SP_EL0, [x28, #-0x10]! /* storing x30, sp */
	stp x26, x27, [x28, #-0x20]!
	ldp x30, x26, [sp, #-0x10] /* loading and storing x28, x29 */
	stp x30, x26, [x28, #0x10]
	/* Now x30 is free for use as LR */
	msr spsel, #0
	mov sp, x28
	mrs X_ESR_EL1, esr_el1
.if \doCheckFPUTrap
	bl _exceptions_checkFPUTrap
.else
	bl _exceptions_storeState
.endif
	b \dispatchFunction
.endm

_exceptions_checkFPUTrap:
#ifndef __SOFTFP__
	lsr w26, W_ESR_EL1, #26
	cmp w26, #0x7 /* SME, SVE, SIMD or FP trapped due to CPACR_EL1 */
	bne _exceptions_storeState
	mrs x26, cpacr_el1
	/* Make sure that FPU is actually trapped (bits 20, 21 are not both 1)
	 * If it is not, treat it as exception */
	tbz x26, #CPACR_FPU_TRAP_BIT0, _exceptions_untrapFPU
	tbz x26, #CPACR_FPU_TRAP_BIT1, _exceptions_untrapFPU
#endif

_exceptions_storeState:
	stp x24, x25, [sp, #-0x10]!
	stp x22, x23, [sp, #-0x10]!
	stp x20, x21, [sp, #-0x10]!
	stp x18, x19, [sp, #-0x10]!
	stp x16, x17, [sp, #-0x10]!
	stp x14, x15, [sp, #-0x10]!
	stp x12, x13, [sp, #-0x10]!
	stp x10, x11, [sp, #-0x10]!
	stp x8, x9, [sp, #-0x10]!
	stp x6, x7, [sp, #-0x10]!
	stp x4, x5, [sp, #-0x10]!
	stp x2, x3, [sp, #-0x10]!
	stp x0, x1, [sp, #-0x10]!
	mrs X_SPSR_EL1, spsr_el1
	mrs x1, elr_el1
	stp X_SPSR_EL1, x1, [sp, #-0x10]!

	mrs x26, cpacr_el1
#ifndef __SOFTFP__
	tbz x26, #CPACR_FPU_TRAP_BIT0, 1f
	tbz x26, #CPACR_FPU_TRAP_BIT1, 1f
	stp q30, q31, [sp, #-0x20]!
	stp q28, q29, [sp, #-0x20]!
	stp q26, q27, [sp, #-0x20]!
	stp q24, q25, [sp, #-0x20]!
	stp q22, q23, [sp, #-0x20]!
	stp q20, q21, [sp, #-0x20]!
	stp q18, q19, [sp, #-0x20]!
	stp q16, q17, [sp, #-0x20]!
	stp q14, q15, [sp, #-0x20]!
	stp q12, q13, [sp, #-0x20]!
	stp q10, q11, [sp, #-0x20]!
	stp q8, q9, [sp, #-0x20]!
	stp q6, q7, [sp, #-0x20]!
	stp q4, q5, [sp, #-0x20]!
	stp q2, q3, [sp, #-0x20]!
	stp q0, q1, [sp, #-0x20]!
	mrs x0, fpcr
	mrs x1, fpsr
	stp x0, x1, [sp, #-0x10]!
	b 2f
1:
	sub sp, sp, #0x210
2:
#endif

	/* Store savesp and trap register */
	sub x1, sp, #0x10
	stp x1, x26, [sp, #-0x10]!
	ret


_exceptions_untrapFPU:
	orr x26, x26, #(3 << 20)
	msr cpacr_el1, x26
	/* Restore the partial state that was stored - no other registers were touched */
	ldp x26, x27, [sp], #0x30
	ldp x30, x29, [sp, #-0x10]
	mov x28, sp
	mov sp, x29
	ldp x28, x29, [x28, #-0x20]
	eret


.globl _exceptions_dispatch
.type _exceptions_dispatch, %function
_exceptions_dispatch:
	/* Assumptions:
	 * sp points at CPU context
	 * X_SPSR_EL1 => SPSR_EL1
	 * X_ESR_EL1 => ESR_EL1
	 * X_SP_EL0 => SP0 before exception
	 */
	/* Get exception syndrome and exception class */
	lsr w0, W_ESR_EL1, #26
	cmp w0, 0x15 /* SVC executed in AArch64 */
	bne not_syscall
	tst X_SPSR_EL1, #MODE_MASK /* ensure coming from EL0 */
	b.eq _syscalls_dispatch

not_syscall:
	/* Not a syscall - store ESR and FAR into exception context */
	mrs x1, far_el1
	stp X_ESR_EL1, x1, [sp, #-0x10]!
	mov x1, sp

	msr daifClr, #7

	/* Arguments to exceptions_dispatch:
	 * * x0 - exception class
	 * * x1 - pointer to exception context
	 */
	bl exceptions_dispatch

	msr daifSet, #7

	ldr x1, [sp, #0x10] /* Discard esr, far - not needed to restore context */
	b _hal_cpuRestoreCtx

_syscalls_dispatch:
	/* Assumptions:
	 * X_ESR_EL1 => ESR_EL1
	 * X_SP_EL0 => userspace SP */
	and x0, X_ESR_EL1, #0xffff
	mov x1, X_SP_EL0
	msr daifClr, #7

	bl syscalls_dispatch

	msr daifSet, #7

	ldr x1, [sp]
	b _hal_cpuRestoreCtx
.size _exceptions_dispatch, .-_exceptions_dispatch


.globl _interrupts_dispatch
.type _interrupts_dispatch, %function
_interrupts_dispatch:
	/* Assumptions:
	 * sp points at CPU context
	 */

	mov x1, sp
	mov x0, #0
	msr daifClr, #5

	/* Arguments to interrupts_dispatch:
	 * * x0 - interrupt number (unused)
	 * * x1 - pointer to CPU context
	 */
	bl interrupts_dispatch

	msr daifSet, #5

	ldr x1, [sp]
	/* Check return value to see if reschedule was performed */
	cbz w0, 1f
	unlock_scheduler
1:
	b _hal_cpuRestoreCtx
.size _interrupts_dispatch, .-_interrupts_dispatch


.globl _hal_cpuRestoreCtx
.type _hal_cpuRestoreCtx, %function
_hal_cpuRestoreCtx:
	/* Assumptions:
	 * x1 => pointer to context to restore from */
	ldr x26, [x1, #0x08]
	add sp, x1, #0x10

	msr cpacr_el1, x26
#ifndef __SOFTFP__
	tbz x26, #CPACR_FPU_TRAP_BIT0, 1f
	tbz x26, #CPACR_FPU_TRAP_BIT1, 1f
	ldp x0, x1, [sp], #0x10
	msr fpcr, x0
	msr fpsr, x1
	ldp q0, q1, [sp], #0x20
	ldp q2, q3, [sp], #0x20
	ldp q4, q5, [sp], #0x20
	ldp q6, q7, [sp], #0x20
	ldp q8, q9, [sp], #0x20
	ldp q10, q11, [sp], #0x20
	ldp q12, q13, [sp], #0x20
	ldp q14, q15, [sp], #0x20
	ldp q16, q17, [sp], #0x20
	ldp q18, q19, [sp], #0x20
	ldp q20, q21, [sp], #0x20
	ldp q22, q23, [sp], #0x20
	ldp q24, q25, [sp], #0x20
	ldp q26, q27, [sp], #0x20
	ldp q28, q29, [sp], #0x20
	ldp q30, q31, [sp], #0x20
	b 2f
1:
	add sp, sp, #0x210
2:
#endif

	ldp x0, x1, [sp], #0x10
	msr spsr_el1, x0
	msr elr_el1, x1
	ldp x0, x1, [sp], #0x10
	ldp x2, x3, [sp], #0x10
	ldp x4, x5, [sp], #0x10
	ldp x6, x7, [sp], #0x10
	ldp x8, x9, [sp], #0x10
	ldp x10, x11, [sp], #0x10
	ldp x12, x13, [sp], #0x10
	ldp x14, x15, [sp], #0x10
	ldp x16, x17, [sp], #0x10
	ldp x18, x19, [sp], #0x10
	ldp x20, x21, [sp], #0x10
	ldp x22, x23, [sp], #0x10
	ldp x24, x25, [sp], #0x10
	ldp x26, x27, [sp], #0x30
	ldp x30, x29, [sp, #-0x10]
	mov x28, sp
	mov sp, x29
	ldp x28, x29, [x28, #-0x20]
	eret
.size _hal_cpuRestoreCtx, .-_hal_cpuRestoreCtx


.globl hal_cpuReschedule
.type hal_cpuReschedule, %function
hal_cpuReschedule:
	msr daifSet, #3
	/* Store CPU registers */
	stp x28, x29, [sp, #-0x20]!
	add x29, sp, #0x20
	stp x30, x29, [sp, #0x10]
	stp x26, x27, [sp, #-0x10]!
	stp x24, x25, [sp, #-0x10]!
	stp x22, x23, [sp, #-0x10]!
	stp x20, x21, [sp, #-0x10]!
	stp x18, x19, [sp, #-0x10]!
	stp x16, x17, [sp, #-0x10]!
	stp x14, x15, [sp, #-0x10]!
	stp x12, x13, [sp, #-0x10]!
	stp x10, x11, [sp, #-0x10]!
	stp x8, x9, [sp, #-0x10]!
	stp x6, x7, [sp, #-0x10]!
	stp x4, x5, [sp, #-0x10]!
	stp x2, x3, [sp, #-0x10]!
	mov x3, #0 /* Push default return value (EOK) as R0 */
	stp x3, x1, [sp, #-0x10]!

	/* TODO: do we need any other part of PSTATE here? */
	mrs x3, currentEL

	/* If spinlock is NULL, skip clearing */
	cbz x0, 1f

	/* Spinlock clear */
	mov w2, #1
	stlrb w2, [x0] /* Global monitor clear generates an event, SEV not necessary */

1:
	/* Push LR as new PC */
	stp x3, x30, [sp, #-0x10]!

	mrs x26, cpacr_el1
#ifndef __SOFTFP__
	tbz x26, #CPACR_FPU_TRAP_BIT0, 1f
	tbz x26, #CPACR_FPU_TRAP_BIT1, 1f
	stp q30, q31, [sp, #-0x20]!
	stp q28, q29, [sp, #-0x20]!
	stp q26, q27, [sp, #-0x20]!
	stp q24, q25, [sp, #-0x20]!
	stp q22, q23, [sp, #-0x20]!
	stp q20, q21, [sp, #-0x20]!
	stp q18, q19, [sp, #-0x20]!
	stp q16, q17, [sp, #-0x20]!
	stp q14, q15, [sp, #-0x20]!
	stp q12, q13, [sp, #-0x20]!
	stp q10, q11, [sp, #-0x20]!
	stp q8, q9, [sp, #-0x20]!
	stp q6, q7, [sp, #-0x20]!
	stp q4, q5, [sp, #-0x20]!
	stp q2, q3, [sp, #-0x20]!
	stp q0, q1, [sp, #-0x20]!
	mrs x0, fpcr
	mrs x1, fpsr
	stp x0, x1, [sp, #-0x10]!
	b 2f
1:
	sub sp, sp, #0x210
2:
#endif

	sub x1, sp, #0x10
	stp x1, x26, [sp, #-0x10]!

	bl threads_schedule

	ldr x1, [sp]
	unlock_scheduler
	b _hal_cpuRestoreCtx
.size hal_cpuReschedule, .-hal_cpuReschedule
.ltorg


.globl hal_jmp /* void hal_jmp(void *f, void *kstack, void *ustack, int kargc, const arg_t *kargs) */
.type hal_jmp, %function
hal_jmp:
	msr daifSet, #3
	/* ustack != NULL means that the jump is to user */
	cbnz x2, .Lu_jmp
	mov x16, x0
	mov sp, x1
	/* pass args */
	subs x3, x3, #1
	b.mi .Lk_jmp
	ldr x0, [x4]
	subs x3, x3, #1
	b.mi .Lk_jmp
	ldr x1, [x4, #8]
	subs x3, x3, #1
	b.mi .Lk_jmp
	ldr x2, [x4, #16]
	subs x3, x3, #1
	b.mi .Lk_jmp
	ldr x3, [x4, #24]
.Lk_jmp:
	msr daifClr, #3
	blr x16
	/* TODO: what if the function above returns? it seems that other implementations of hal_jmp also don't handle this */
	udf #0 /* Cause an exception if the function returns */

	/* Handle userspace jump */
.Lu_jmp:
	mov sp, x2
	msr elr_el1, x16
	mov x1, #MODE_EL0
	msr spsr_el1, x1
	eret
.size hal_jmp, .-hal_jmp
.ltorg


.align 11
/* Vector Table Definition */
.globl _vector_table
_vector_table:
/* from EL1 with SP_EL0 */
.org(_vector_table)
	exception_vector 0 1 _exceptions_dispatch

.org (_vector_table + 0x80)
	exception_vector 0 0 _interrupts_dispatch

.org (_vector_table + 0x100)
	exception_vector 0 0 _interrupts_dispatch

.org (_vector_table + 0x180)
	exception_vector 0 0 _exceptions_dispatch

/* from EL1 with SP_EL1 */
.org (_vector_table + 0x200)
	b .

.org (_vector_table + 0x280)
	b .

.org (_vector_table + 0x300)
	b .

.org (_vector_table + 0x380)
	b .

/* from EL0 in AArch64 */
.org (_vector_table + 0x400)
	exception_vector 1 1 _exceptions_dispatch

.org (_vector_table + 0x480)
	exception_vector 1 0 _interrupts_dispatch

.org (_vector_table + 0x500)
	exception_vector 1 0 _interrupts_dispatch

.org (_vector_table + 0x580)
	exception_vector 1 0 _exceptions_dispatch

/* from EL0 in AArch32 */
.org (_vector_table + 0x600)
	b .

.org (_vector_table + 0x680)
	b .

.org (_vector_table + 0x700)
	b .

.org (_vector_table + 0x780)
	b .

.org (_vector_table + 0x800)
.ltorg
