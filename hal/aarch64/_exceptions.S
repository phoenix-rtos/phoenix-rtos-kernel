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
#define X_SPSR_EL1  x3
#define X_CPACR_EL1 x27
#define X_ESR_EL1   x28
#define W_ESR_EL1   w28
#define X_OLD_SP    x29

.extern schedulerLocked

.macro unlock_scheduler scratchReg
	adr \scratchReg, schedulerLocked
	stlr wzr, [\scratchReg] /* Global monitor clear generates an event, SEV not necessary */
.endm


/* Unused vectors are filled with undefined instructions to cause an exception if they are ever triggered */
.macro unused_vector
.rept 31
	udf #0
.endr
	b .
.endm


/* This macro creates an exception vector, so it cannot contain more than 32 instructions */
.macro exception_vector fromUserspace doCheckFPUTrap dispatchFunction
	stp x28, x29, [sp, #-0x20]
.if \fromUserspace
	mrs X_OLD_SP, sp_el0
.else
	mov X_OLD_SP, sp
.endif
	stp x30, X_OLD_SP, [sp, #-0x10]
	stp x26, x27, [sp, #-0x30]!
	mrs X_CPACR_EL1, cpacr_el1
	mrs X_ESR_EL1, esr_el1
.if \doCheckFPUTrap
	bl _exceptions_checkFPUTrap
.else
	bl _exceptions_storeState
.endif
	b \dispatchFunction
.endm


.align 4
_exceptions_checkFPUTrap:
#ifndef __SOFTFP__
	lsr w26, W_ESR_EL1, #26
	cmp w26, #0x7 /* SME, SVE, SIMD or FP trapped due to CPACR_EL1 */
	bne _exceptions_storeState
	/* Make sure that FPU is actually trapped (bits 20, 21 are not both 1)
	 * If it is not, treat it as exception */
	tbz X_CPACR_EL1, #CPACR_FPU_TRAP_BIT0, _exceptions_untrapFPU
	tbz X_CPACR_EL1, #CPACR_FPU_TRAP_BIT1, _exceptions_untrapFPU
#endif

_exceptions_storeState:
	sub sp, sp, #0xe0
	stp x24, x25, [sp, #0xd0]
	stp x22, x23, [sp, #0xc0]
	stp x20, x21, [sp, #0xb0]
	stp x18, x19, [sp, #0xa0]
	stp x16, x17, [sp, #0x90]
	stp x14, x15, [sp, #0x80]
	stp x12, x13, [sp, #0x70]
	stp x10, x11, [sp, #0x60]
	stp x8, x9, [sp, #0x50]
	stp x6, x7, [sp, #0x40]
	stp x4, x5, [sp, #0x30]
	stp x2, x3, [sp, #0x20]
	stp x0, x1, [sp, #0x10]
	mrs X_SPSR_EL1, spsr_el1
	mrs x1, elr_el1
	stp X_SPSR_EL1, x1, [sp]

	sub sp, sp, 0x220
#ifndef __SOFTFP__
	tbz X_CPACR_EL1, #CPACR_FPU_TRAP_BIT0, 1f
	tbz X_CPACR_EL1, #CPACR_FPU_TRAP_BIT1, 1f
	stp q30, q31, [sp, #0x200]
	stp q28, q29, [sp, #0x1e0]
	stp q26, q27, [sp, #0x1c0]
	stp q24, q25, [sp, #0x1a0]
	stp q22, q23, [sp, #0x180]
	stp q20, q21, [sp, #0x160]
	stp q18, q19, [sp, #0x140]
	stp q16, q17, [sp, #0x120]
	stp q14, q15, [sp, #0x100]
	stp q12, q13, [sp, #0xe0]
	stp q10, q11, [sp, #0xc0]
	stp q8, q9, [sp, #0xa0]
	stp q6, q7, [sp, #0x80]
	stp q4, q5, [sp, #0x60]
	stp q2, q3, [sp, #0x40]
	stp q0, q1, [sp, #0x20]
	mrs x0, fpcr
	mrs x1, fpsr
	stp x0, x1, [sp, #0x10]
1:
#endif

	/* Store savesp and trap register */
	mov x1, sp
	stp x1, X_CPACR_EL1, [sp]
	ret

_exceptions_untrapFPU:
	orr X_CPACR_EL1, X_CPACR_EL1, #(3 << 20)
	msr cpacr_el1, X_CPACR_EL1
	isb

	/* Initialize all registers with NAN - this prevents possible data leaks */
	adr x26, .L_two_nans
	ldp q0, q1, [x26]
	ldp q2, q3, [x26]
	ldp q4, q5, [x26]
	ldp q6, q7, [x26]
	ldp q8, q9, [x26]
	ldp q10, q11, [x26]
	ldp q12, q13, [x26]
	ldp q14, q15, [x26]
	ldp q16, q17, [x26]
	ldp q18, q19, [x26]
	ldp q20, q21, [x26]
	ldp q22, q23, [x26]
	ldp q24, q25, [x26]
	ldp q26, q27, [x26]
	ldp q28, q29, [x26]
	ldp q30, q31, [x26]

	/* Restore the partial state that was stored - no other registers were touched */
	ldp x26, x27, [sp]
	ldp x28, x29, [sp, #0x10]
	ldp x30, xzr, [sp, #0x20] /* Stored SP is ignored here */
	add sp, sp, #0x30
	eret

.align 4
.type .L_two_nans, %object
.L_two_nans:
.word 0
.word 0x7ff80000
.word 0
.word 0
.word 0
.word 0x7ff80000
.word 0
.word 0

.align 4
.globl _exceptions_dispatch
.type _exceptions_dispatch, %function
_exceptions_dispatch:
	/* Extract exception class from syndrome */
	lsr w0, W_ESR_EL1, #26
	cmp w0, 0x15 /* SVC executed in AArch64 */
	b.eq _syscalls_dispatch

	/* Not a syscall - store ESR and FAR into exception context */
	mrs x1, far_el1
	stp X_ESR_EL1, x1, [sp, #-0x10]!
	mov x1, sp

	msr daifClr, #7

	/* Arguments to exceptions_dispatch:
	 * * x0 - exception class
	 * * x1 - pointer to exception context */
	bl exceptions_dispatch

	msr daifSet, #7

	ldr x1, [sp, #0x10] /* Discard esr, far - not needed to restore context */
	b _hal_cpuRestoreCtx

_syscalls_dispatch:
	and x0, X_ESR_EL1, #0xffff
	tst X_SPSR_EL1, #MODE_MASK /* check if coming from EL0 */
	b.ne .L_el1_syscall
	mov x1, X_OLD_SP
	mov x2, sp
	msr daifClr, #7

	/* Arguments to syscalls_dispatch:
	 * * x0 - syscall number
	 * * x1 - userspace stack pointer
	 * * x2 - pointer to exception context */
	bl syscalls_dispatch

	msr daifSet, #7

	ldr x1, [sp]
	b _hal_cpuRestoreCtx

.L_el1_syscall:
	/* Caller wants to reschedule */
	bic X_SPSR_EL1, X_SPSR_EL1, #NO_INT /* Re-enable interrupts in the caller */
	str X_SPSR_EL1, [sp, 0x220]

	ldr x2, [sp, #0x230]  /* Load x0 -> pointer to spinlock */
	cbz x2, 1f /* If pointer not NULL, clear the spinlock - but don't restore the DAIF */
	mov w1, #1
	stlrb w1, [x2] /* Global monitor clear generates an event, SEV not necessary */
1:
	str xzr, [sp, #0x230] /* Store 0 into x0 as return value (EOK) */

	mov x1, sp /* argument to function - cpu_context_t* */

	/* Arguments to threads_schedule:
	 * * x0 - n (unused)
	 * * x1 - pointer to CPU context
	 * * x2 - arg (unused)*/
	bl threads_schedule

	ldr x1, [sp]
	unlock_scheduler x0
	b _hal_cpuRestoreCtx
.size _exceptions_dispatch, .-_exceptions_dispatch


.align 4
.globl _interrupts_dispatch
.type _interrupts_dispatch, %function
_interrupts_dispatch:
	mov x1, sp
	mov x0, #0
	msr daifClr, #4

	/* Arguments to interrupts_dispatch:
	 * * x0 - interrupt number (unused)
	 * * x1 - pointer to CPU context */
	bl interrupts_dispatch

	msr daifSet, #4

	ldr x1, [sp]
	/* Check return value to see if reschedule was performed */
	cbz w0, 1f
	unlock_scheduler x0
1:
	b _hal_cpuRestoreCtx
.size _interrupts_dispatch, .-_interrupts_dispatch


.align 4
.globl _hal_cpuRestoreCtx
.type _hal_cpuRestoreCtx, %function
_hal_cpuRestoreCtx:
	/* Assumptions:
	 * x1 => pointer to context to restore from */
	ldr X_CPACR_EL1, [x1, #0x08]
	msr cpacr_el1, X_CPACR_EL1
	isb
#ifndef __SOFTFP__
	tbz X_CPACR_EL1, #CPACR_FPU_TRAP_BIT0, 1f
	tbz X_CPACR_EL1, #CPACR_FPU_TRAP_BIT1, 1f
	ldp x2, x3, [x1, #0x10]
	msr fpcr, x2
	msr fpsr, x3
	ldp q0, q1, [x1, #0x20]
	ldp q2, q3, [x1, #0x40]
	ldp q4, q5, [x1, #0x60]
	ldp q6, q7, [x1, #0x80]
	ldp q8, q9, [x1, #0xa0]
	ldp q10, q11, [x1, #0xc0]
	ldp q12, q13, [x1, #0xe0]
	ldp q14, q15, [x1, #0x100]
	ldp q16, q17, [x1, #0x120]
	ldp q18, q19, [x1, #0x140]
	ldp q20, q21, [x1, #0x160]
	ldp q22, q23, [x1, #0x180]
	ldp q24, q25, [x1, #0x1a0]
	ldp q26, q27, [x1, #0x1c0]
	ldp q28, q29, [x1, #0x1e0]
	ldp q30, q31, [x1, #0x200]
1:
#endif
	add x1, x1, #0x220

	ldp x0, x4, [x1]
	msr spsr_el1, x0
	msr elr_el1, x4

	mrs x5, tpidr_el1
	/* If stored state was in EL0, SP_EL1 needs to be set to bottom of kernel stack
	 * and the next write to sp will restore SP_EL0.
	 * If stored state was in EL1, this write to sp is incorrect,
	 * but next write to sp will correctly restore SP_EL1. */
	mov sp, x5
	and x0, x0, #1

	ldp x2, x3, [x1, #0x20]
	ldp x4, x5, [x1, #0x30]
	ldp x6, x7, [x1, #0x40]
	ldp x8, x9, [x1, #0x50]
	ldp x10, x11, [x1, #0x60]
	ldp x12, x13, [x1, #0x70]
	ldp x14, x15, [x1, #0x80]
	ldp x16, x17, [x1, #0x90]
	ldp x18, x19, [x1, #0xa0]
	ldp x20, x21, [x1, #0xb0]
	ldp x22, x23, [x1, #0xc0]
	ldp x24, x25, [x1, #0xd0]
	ldp x26, x27, [x1, #0xe0]
	ldp x28, x29, [x1, #0xf0]

	msr spsel, x0
	ldp x30, x0, [x1, #0x100]
	mov sp, x0 /* Restore sp into either SP_EL1 or SP_EL0 depending on stored state */

	ldp x0, x1, [x1, #0x10]
	eret
.size _hal_cpuRestoreCtx, .-_hal_cpuRestoreCtx
.ltorg


.align 4
.globl hal_cpuReschedule
.type hal_cpuReschedule, %function
hal_cpuReschedule:
	/* hal_cpuReschedule is implemented as an "EL1 syscall" */
	svc 0
	ret
.size hal_cpuReschedule, .-hal_cpuReschedule


.align 4
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
	/* The function should not return; cause an exception if it does */
	udf #0

	/* Handle userspace jump */
.Lu_jmp:
	msr elr_el1, x0
	mrs x1, tpidr_el1
	mov sp, x1
	msr sp_el0, x2
	/* TODO: There are some extensions that have their flags in SPSR and we clear them here. */
	mov x1, #MODE_EL0
	msr spsr_el1, x1
	eret
.size hal_jmp, .-hal_jmp


.align 11, 0x00
/* Vector Table Definition */
.globl _vector_table
_vector_table:
/* from EL1 with SP_EL0 */
.org(_vector_table)
	unused_vector

.org (_vector_table + 0x80)
	unused_vector

.org (_vector_table + 0x100)
	unused_vector

.org (_vector_table + 0x180)
	unused_vector

/* from EL1 with SP_EL1 */
.org (_vector_table + 0x200)
	exception_vector 0 1 _exceptions_dispatch

.org (_vector_table + 0x280)
	exception_vector 0 0 _interrupts_dispatch

.org (_vector_table + 0x300)
	exception_vector 0 0 _interrupts_dispatch

.org (_vector_table + 0x380)
	exception_vector 0 0 _exceptions_dispatch

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
	unused_vector

.org (_vector_table + 0x680)
	unused_vector

.org (_vector_table + 0x700)
	unused_vector

.org (_vector_table + 0x780)
	unused_vector

.org (_vector_table + 0x800)
.ltorg
