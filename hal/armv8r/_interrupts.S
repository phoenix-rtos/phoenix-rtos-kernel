/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupts handlers for ARMv8-R
 *
 * Copyright 2018, 2020, 2021, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Maciej Purski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

.arm

.extern schedulerLocked

.macro push_fpu_state reg_tmp
	vpush {d16-d31}
	vpush {d0-d15}
	vmrs \reg_tmp, fpscr
	push {\reg_tmp}
.endm

.macro unlock_scheduler
	ldr r0, =schedulerLocked
	mov r1, #0
	dmb
	str r1, [r0]
	dmb
	isb
.endm

.globl _exception_undef
.type _exception_undef, %function
_exception_undef:
	cpsid if
	stmfd sp, {r0-r4}
	mov r0, #1 /* exc_undef */
	mrs r3, spsr
	tst r3, #THUMB_STATE
	subeq r2, lr, #4
	subne r2, lr, #2
	b _exceptions_dispatch
.size _exception_undef, .-_exception_undef


.globl _exception_prefetch
.type _exception_prefetch, %function
_exception_prefetch:
	cpsid if
	stmfd sp, {r0-r4}
	mov r0, #3 /* exc_prefetch */
	sub r2, lr, #4
	b _exceptions_dispatch
.size _exception_prefetch, .-_exception_prefetch


.globl _exception_abort
.type _exception_abort, %function
_exception_abort:
	cpsid if
	stmfd sp, {r0-r4}
	mov r0, #4 /* exc_abort */
	sub r2, lr, #8
	b _exceptions_dispatch
.size _exception_abort, .-_exception_abort


.globl _exceptions_dispatch
.type _exceptions_dispatch, %function
_exceptions_dispatch:
	/* Contents of registers:
	 * r0 - exception number
	 * r2 - PC of the instruction that caused the exception
	 * registers r0-r4 from previous context are saved below SP */
	mrs r3, spsr
	sub r1, sp, #0x14
	mrc p15, 0, r4, c13, c0, 4 /* TPIDRPRW */
	cps #MODE_SYS
	tst r3, #0x0f
	movne r4, sp
	stmfd r4!, {r2}
	stmfd r4!, {r5-r14}
	mov sp, r4
	ldmfd r1, {r4-r8}
	push {r3-r8}

	push_fpu_state r4

	sub sp, sp, #8
	str sp, [sp]

	/* Push exception context */
	mrc p15, 0, r1, c6, c0, 2 /* IFAR */
	push {r1}
	mrc p15, 0, r1, c5, c0, 1 /* IFSR */
	push {r1}
	mrc p15, 0, r1, c6, c0, 0 /* DFAR */
	push {r1}
	mrc p15, 0, r1, c5, c0, 0 /* DFSR */
	push {r1}

	mov r1, sp

	blx exceptions_dispatch

	ldr sp, [sp, #0x10]
	add sp, sp, #8
	b _hal_cpuRestoreCtx
.size _exceptions_dispatch, .-_exceptions_dispatch


.globl _hal_cpuRestoreCtx
.type _hal_cpuRestoreCtx, %function
_hal_cpuRestoreCtx:
	/* CLREX should be executed as part of context switch */
	clrex

	/* Restore fpu context */
	pop {r4}
	vmsr fpscr, r4
	vpop {d0-d15}
	vpop {d16-d31}

	pop {r11} /* r11 - apsr */
	pop {r0-r10}
	mov r12, sp /* r12 - points saved r11, r12, sp, lr, pc */
	ldr sp, [r12, #0x8]
	ldr lr, [r12, #0xc]
	cps #MODE_IRQ
	push {r11} /* cpsr saved, r11 free */
	ldr r11, [r12, #0x0]
	ldr lr, [r12, #0x10]
	push {lr}
	ldr r12, [r12, #0x4]
	rfefd sp! /* return from exception - pops pc and cpsr */
.size _hal_cpuRestoreCtx, .-_hal_cpuRestoreCtx


.globl hal_cpuReschedule
.type hal_cpuReschedule, %function
hal_cpuReschedule:
	cpsid if
	/* Store CPU registers */
	str sp, [sp, #-12]
	push {lr} /* Push LR as both PC and LR */
	push {lr}
	sub sp, #4 /* Skip over SP, already saved */
	push {r1-r12}
	mov r3, #0
	push {r3} /* Push default return value (EOK) as R0 */

	mrs r4, cpsr

	cmp r0, #0
	beq 1f

	add r0, #12

	/* Spinlock clear */
	dmb
spinlock:
	ldrexb r3, [r0]
	add r3, r3, #1
	strexb r2, r3, [r0]
	cmp r2, #0
	bne spinlock
	ldrb r1, [r1]

	bic r4, #0xff
	and r1, #0xff
	orr r4, r4, r1
1:
	/* store CPSR with adjusted I, F, T flags */
	bic r4, #0xe0
	and r5, lr, #1 /* extract Thumb flag from LR address */
	orr r4, r4, r5, lsl #5
	push {r4}

	push_fpu_state r4

	sub r1, sp, #8
	push {r1}
	push {r1}

	blx threads_schedule

	ldr sp, [sp]
	add sp, sp, #8

	unlock_scheduler
	b _hal_cpuRestoreCtx
.size hal_cpuReschedule, .-hal_cpuReschedule


.globl _interrupts_dispatch
.type _interrupts_dispatch, %function
_interrupts_dispatch:
	stmfd sp, {r0-r3}
	mrs r2, spsr
	sub r1, lr, #4
	sub r0, sp, #0x10
	/* fetch kernel thread SP from TPIDRPRW register */
	mrc p15, 0, r3, c13, c0, 4

	/* return to SYS mode with no interrupts */
	cpsie af, #MODE_SYS

	/* If exception was not taken in user mode, use current stack
	 * to store context. Otherwise use preferred one from r3 */
	tst r2, #0x0f
	movne r3, sp

	/* save return address */
	stmfd r3!, {r1}

	/* store original r4-r14 registers as in hal_cpuReschedule()
	 * (original r0-r3 are still on exception stack) */
	stmfd r3!, {r4-r14}
	mov sp, r3

	/* fetch original r0-r3 from exception stack and store on local one
	 * including SPSR stored in current r3 */
	ldmfd r0, {r3-r6}
	push {r2-r6}

	push_fpu_state r4

	/* save SP on top of the stack and pass it as arg1 to IRQ handler (it is cpu_context_t *) */
	sub r1, sp, #8
	push {r1}
	push {r1}

	/* pass dummy data to first argument to be consistent with interface
	 * irq number is calculated in interrupts_dispatch function */
	mov r0, #0

	blx interrupts_dispatch

	ldr sp, [sp]
	add sp, sp, #8

	cmp r0, #0
	beq 1f
	unlock_scheduler
1:
	b _hal_cpuRestoreCtx
.size _interrupts_dispatch, .-_interrupts_dispatch


.globl _syscalls_dispatch
.type _syscalls_dispatch, %function
_syscalls_dispatch:
	stmfd sp, {r0-r4}^
	sub r2, sp, #0x14
	mrs r3, spsr
	mov r1, lr
	tst r3, #THUMB_STATE
	ldreq r0, [r1, #-4]
	biceq r0, r0, #0xff000000
	ldrneh r0, [r1, #-2]
	bicne r0, r0, #0xff00
	mrc p15, 0, r4, c13, c0, 4 /* TPIDRPRW */

	cpsie af, #MODE_SYS

	/* Store CPU state onto kernel stack */
	stmfd r4!, {r1}
	stmfd r4!, {r5-r14}
	mov r1, sp
	mov sp, r4
	ldmfd r2, {r4-r8}
	push {r3-r8}
	push_fpu_state r4
	sub r2, sp, #8
	push {r2}
	push {r2}

	cpsie if

	blx syscalls_dispatch

	cpsid if

	ldr sp, [sp]
	add sp, sp, #8
	b _hal_cpuRestoreCtx
.size _syscalls_dispatch, .-_syscalls_dispatch
