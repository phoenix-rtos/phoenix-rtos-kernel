/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupts handlers for Cortex-A9
 *
 * Copyright 2018, 2020, 2021 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Maciej Purski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

.extern schedulerLocked

.macro unlock_scheduler
	ldr x0, =schedulerLocked
	stlrb wzr, [x0] /* Global monitor clear generates an event, SEV not necessary */
.endm

/* This macro creates an exception vector, so it cannot contain more than 32 instructions */
.macro exception_vector fromUserspace doCheckFPUTrap dispatchFunction
	stp x28, x29, [sp, #-0x10]
	stp x26, x27, [sp, #-0x20]
	mrs x29, sp_el0
.if \fromUserspace
	mrs x27, tpidr_el1
.else
	mov x27, x29
.endif
	mrs x28, esr_el1
	stp x30, x29, [x27, #-0x10]! /* storing x30, sp */
	ldp x30, x26, [sp, #-0x10] /* loading and storing x28, x29 */
	stp x30, x26, [x27, #-0x10]!
	ldp x30, x26, [sp, #-0x20] /* loading and storing x26, x27 */
	stp x30, x26, [x27, #-0x10]!
	/* Now x30 is free for use as LR */
	msr spsel, #0
	mov sp, x27
.if \doCheckFPUTrap
	bl _exceptions_checkFPUTrap
.else
	bl _exceptions_storeState
.endif
	b \dispatchFunction
.endm

_exceptions_checkFPUTrap:
	lsr w26, w28, #26
	cmp w26, #0x7 /* SME, SVE, SIMD or FP trapped due to CPACR_EL1 */
	bne _exceptions_storeState
	mrs x26, cpacr_el1
	/* Make sure that FPU is actually trapped (bits 20, 21 are not both 1)
	 * If it is not, treat it as exception */
	tbz x26, #20, _exceptions_untrapFPU
	tbz x26, #21, _exceptions_untrapFPU

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
	mrs x3, spsr_el1
	mrs x1, elr_el1
	stp x3, x1, [sp, #-0x10]!

	/* TODO: store FP/SIMD registers - for now just reserve space */
	sub sp, sp, #0x210

	/* Store savesp */
	sub x1, sp, #0x10
	stp x1, xzr, [sp, #-0x10]!
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
	 * x3 => SPSR_EL1
	 * x28 => ESR_EL1
	 * x29 => SP0 before exception
	 */
	/* Get exception syndrome and exception class */
	lsr w0, w28, #26
	cmp w0, 0x15 /* SVC executed in AArch64 */
	bne not_syscall
	tst x3, #MODE_MASK /* ensure coming from EL0 */
	beq _syscalls_dispatch

not_syscall:
	/* Not a syscall - store ESR and FAR into exception context */
	mrs x1, far_el1
	stp x28, x1, [sp, #-0x10]!
	mov x1, sp

	msr daifClr, #7

	/* Arguments to exceptions_dispatch:
	 * * x0 - exception class
	 * * x1 - pointer to exception context
	 */
	bl exceptions_dispatch

	msr daifSet, #7

	add sp, sp, #0x10 /* Discard esr, far - not needed to restore context */
	b _hal_cpuRestoreCtx

_syscalls_dispatch:
	/* Assumptions:
	 * x2 => ESR
	 * x29 => userspace SP */
	and x0, x2, #0xffff
	mov x1, x29
	msr daifClr, #7

	bl syscalls_dispatch

	msr daifSet, #7

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

	/* Check return value to see if reschedule was performed */
	cbz w0, 1f
	unlock_scheduler
1:
	b _hal_cpuRestoreCtx
.size _interrupts_dispatch, .-_interrupts_dispatch


.globl _hal_cpuRestoreCtx
.type _hal_cpuRestoreCtx, %function
_hal_cpuRestoreCtx:
	ldr x1, [sp]
	mov sp, x1
	add sp, sp, #0x10

	/* TODO: restore FP/SIMD registers - for now just skip */
	add sp, sp, #0x210

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
	stp x28, x29, [sp, #-0x20]
	mov x29, sp
	stp x30, x29, [sp, #-0x10]!
	stp x26, x27, [sp, #-0x20]! /* Skip over x28, x29 */
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
	add x0, x0, #24
	stlrb w2, [x0] /* Global monitor clear generates an event, SEV not necessary */

1:
	/* Push LR as new PC */
	stp x3, x30, [sp, #-0x10]!

	/* TODO: store FP/SIMD registers - for now just reserve space */
	sub sp, sp, #0x210

	sub x1, sp, #0x10
	stp x1, xzr, [sp, #-0x10]!

	bl threads_schedule

	unlock_scheduler
	b _hal_cpuRestoreCtx
.size hal_cpuReschedule, .-hal_cpuReschedule
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

/* from EL0 in AArch32 */
.org (_vector_table + 0x400)
	b .

.org (_vector_table + 0x480)
	b .

.org (_vector_table + 0x500)
	b .

.org (_vector_table + 0x580)
	b .

/* from EL0 in AArch64 */
.org (_vector_table + 0x600)
	exception_vector 1 1 _exceptions_dispatch

.org (_vector_table + 0x680)
	exception_vector 1 0 _interrupts_dispatch

.org (_vector_table + 0x700)
	exception_vector 1 0 _interrupts_dispatch

.org (_vector_table + 0x780)
	exception_vector 1 0 _exceptions_dispatch

.org (_vector_table + 0x800)
.ltorg
