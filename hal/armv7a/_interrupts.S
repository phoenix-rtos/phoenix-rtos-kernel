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

#include "cpu.h"

.arm

.globl _exception_undef
.type _exception_undef, %function
_exception_undef:
	cpsid if
	stmfd sp, {r0-r4}
	mov r0, #1
	mrs r3, spsr
	tst r3, #0x20
	subeq r2, lr, #4
	subne r2, lr, #2
	b _exceptions_dispatch
.size _exception_undef, .-_exception_undef


.globl _exception_prefetch
.type _exception_prefetch, %function
_exception_prefetch:
	cpsid if
	stmfd sp, {r0-r4}
	mov r0, #3
	sub r2, lr, #4
	b _exceptions_dispatch
.size _exception_prefetch, .-_exception_prefetch


.globl _exception_abort
.type _exception_abort, %function
_exception_abort:
	cpsid if
	stmfd sp, {r0-r4}
	mov r0, #4
	sub r2, lr, #8
.size _exception_abort, .-_exception_abort


.globl _exceptions_dispatch
.type _exceptions_dispatch, %function
_exceptions_dispatch:
	mrs r3, spsr
	sub r1, sp, #0x14
	mrc p15, 0, r4, c13, c0, 4
	cps #SYS_MODE
	tst r3, #0x0f
	movne r4, sp
	stmfd r4!, {r2}
	stmfd r4!, {r5-r14}
	mov sp, r4
	ldmfd r1, {r4-r8}
	push {r3-r8}
	mrc p15, 0, r1, c6, c0, 2
	push {r1}
	mrc p15, 0, r1, c5, c0, 1
	push {r1}
	mrc p15, 0, r1, c6, c0, 0
	push {r1}
	mrc p15, 0, r1, c5, c0, 0
	push {r1}
	sub r1, sp, #4
	push {r1}

	ldr lr, =exceptions_dispatch
	blx lr

	ldr sp, [sp]
	add sp, sp, #20

	pop {r11}
	pop {r0-r10}
	mov r12, sp
	ldr sp, [r12, #0x8]
	ldr lr, [r12, #0xc]
	cps #IRQ_MODE
	push {r11}
	ldr r11, [r12, #0x0]
	ldr lr, [r12, #0x10]
	push {lr}
	ldr r12, [r12, #0x4]
	rfefd sp!
.size _exceptions_dispatch, .-_exceptions_dispatch


.globl _hal_cpuRestoreCtx
.type _hal_cpuRestoreCtx, %function
_hal_cpuRestoreCtx:
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
	cps #IRQ_MODE
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
	push {lr}
	stmfd sp, {r0-r14}
	sub sp, sp, #0x3c

	mov r3, r1 /* Save spinlock context for later */

	/* Default return value - EOK */
	mov r1, #0
	str r1, [sp]

	ldr r1, [sp, #0x34]
	add r1, #4
	str r1, [sp, #0x34]

	mrs r4, cpsr

	cmp r0, #NULL
	beq 1f

	add r0, #12

	/* Spinlock clear */
	ldrexb r1, [r0]
	add r1, r1, #1
	dmb
	strexb r2, r1, [r0]
	ldrb r1, [r3]

	bic r4, #0xff
	and r1, #0xff
	orr r4, r4, r1
1:
	/* store CPSR with adjusted M and I flags */
	and r5, lr, #1 /* extract Thumb flag from LR address */
	orr r4, r4, r5, lsl #5
	bic r4, #0xc0
	push {r4}

	/* Store fpu context */
	vpush {d16-d31}
	vpush {d0-d15}
	vmrs r4,fpscr
	push {r4}

	sub r1, sp, #8
	push {r1}
	push {r1}

	blx threads_schedule

	ldr sp, [sp]
	add sp, sp, #8
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
	cpsie af, #SYS_MODE

	/* If exception was not taken in user mode, use curren stack
	 * to store context. Otherwise use preffered one from r3 */
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

	/* Store fpu context */
	vpush {d16-d31}
	vpush {d0-d15}
	vmrs r4,fpscr
	push {r4}

	/* save SP on top of the stack and pass it as arg1 to IRQ handler (it is cpu_context_t *) */
	sub r1, sp, #8
	push {r1}
	push {r1}

	ldr r2, =VADDR_GIC + 0x2000 /* Interrupt Acknowledge register */
	lsr r2, r2, #12
	lsl r2, r2, #12
	ldr r4, [r2, #0x0c] /* r4[12:10] = CPU_ID of software interrupts, r4[9:0] = INT_ID */
	ldr r3, =0x03fe
	and r2, r3, r4
	cmp r2, r3 /* Check that INT_ID is not 1023 or 1022 (spurious interrupt) */
	beq 1f
	add r3, r3, #1

	/* pass INT_ID as arg0 (in r0) to the IRQ handler */
	and r0, r3, r4

	blx interrupts_dispatch

	ldr r0, =VADDR_GIC + 0x2000
	lsr r0, r0, #12
	lsl r0, r0, #12
	str r4, [r0, #0x10] /* Update End of Interrupt register with original value from ICCIAR */
1:
	ldr sp, [sp]
	add sp, sp, #8
	b _hal_cpuRestoreCtx
.size _interrupts_dispatch, .-_interrupts_dispatch


.globl _syscalls_dispatch
.type _syscalls_dispatch, %function
_syscalls_dispatch:
	stmfd sp, {r0-r4}^
	sub r1, sp, #0x14
	mrs r3, spsr
	mov r2, lr
	tst r3, #THUMB_STATE
	ldreq r0, [r2, #-4]
	biceq r0, r0, #0xff000000
	ldrneh r0, [r2, #-2]
	bicne r0, r0, #0xff00
	mrc p15, 0, r4, c13, c0, 4

	cpsie af, #SYS_MODE

	stmfd r4!, {r2}
	stmfd r4!, {r5-r14}
	mov r2, sp
	mov sp, r4
	ldmfd r1, {r4-r8}
	push {r3-r8}
	vpush {d16-d31}
	vpush {d0-d15}
	vmrs r4,fpscr
	push {r4}
	sub r1, sp, #8
	push {r1}
	push {r1}

	mov r1, r2

	cpsie if

	blx syscalls_dispatch

	cpsid if

	str r0, [sp, #272]

	ldr sp, [sp]
	add sp, sp, #8
	b _hal_cpuRestoreCtx
.size _syscalls_dispatch, .-_syscalls_dispatch
