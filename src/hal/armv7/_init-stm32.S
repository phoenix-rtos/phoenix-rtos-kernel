/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for Cortex-M3/M4 (ARMv7) architecture
 *
 * Copyright 2012, 2016-2017 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "cpu.h"

.syntax unified
.cpu cortex-m3
.thumb


.extern _cpu_nctx
.extern syscalls
.extern syspage


.section .init, "x"

.globl _init_vectors
.type _init_vectors, %function
_init_vectors:
.word _end + 1024 + 256
.word _start

.word _exceptions_dispatch /* NMI */
.word _exceptions_dispatch /* HardFault */
.word _exceptions_dispatch /* MemMgtFault */
.word _exceptions_dispatch /* BusFault */
.word _exceptions_dispatch /* UsageFault */
.word 0
.word 0
.word 0
.word 0
.word _syscall_dispatch    /* SVC */
.word _exceptions_dispatch /* Debug */
.word 0
.word _interrupts_dispatch /* PendSV */
.word _interrupts_dispatch /* Systick */

.word _interrupts_dispatch /* IRQ0 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ8 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ16 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ24 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ32 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ40 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ48 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ56 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ64 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.size _init_vectors, .-_init_vectors


.globl _start
.type _start, %function

_start:
	cpsid if

	/* Init vector table pointer */
	ldr r0, =0xe000ed08
	ldr r1, =_init_vectors
	bic r1, #1
	str r1, [r0]
	bl _stm32_init

	/* Pass syspage to main */
	ldr r0, =_init_vectors
	and r0, r0, #-2
	add r0, r0, #0x10000
	ldr r1, =syspage
	str r0, [r1]
	bl main
.size _start, .-_start


.globl _syscall_dispatch
.type _syscall_dispatch, %function

_syscall_dispatch:
	/* Find out which stack has arguments */
	tst lr, #4
	ite eq
	mrseq r0, msp
	mrsne r0, psp

	push {lr}

	/* Get syscall number and its function pointer */
	ldr r1, =syscalls
	ldr r2, [r0, #24]
	ldrb r2, [r2, #-2]
	lsl r2, #2
	ldr r1, [r1, r2]

	/* Prepare pseudo context */
	mov r2, #0x01000000
	push {r2} /* PSR */
	push {r1} /* PC */
	mov r2, _syscallend - _syscallrel - 1
	add r2, r2, pc
_syscallrel:
	push {r2} /* LR */
	mov r2, #0
	push {r2} /* R12 */
	push {r2} /* R3 */
	push {r2} /* R2 */
	push {r2} /* R1 */
	push {r0} /* R0 */

	/* Exit handler mode to kernel thread mode */
	mov lr, #0
	msr control, lr
	mov lr, #0xfffffff9
	cpsie if
	bx lr

_syscallend:
	/* There is still original context on stack and return value in r0 register */
	pop {lr}

	tst lr, #4
	ite eq
	moveq r1, #0
	movne r1, #3
	msr control, r1

	ldr r1, [sp, #24]
	orr r1, r1, #1
	str r1, [sp, #24]

	pop {r1} /* sic! */
	pop {r1}
	pop {r2}
	pop {r3}
	pop {r12}
	pop {lr}
	ldr pc, [sp], #8
.size _syscall_dispatch, .-_syscall_dispatch

.globl _exceptions_dispatch
.type _exceptions_dispatch, %function

_exceptions_dispatch:
	cpsid if

	push {r4-r11}
	mrs r0, psp
	push {r0}

	mrs r0, ipsr
	mov r1, sp
	b exceptions_dispatch
.size _exceptions_dispatch, .-_exceptions_dispatch

.globl _interrupts_dispatch
.type _interrupts_dispatch, %function

_interrupts_dispatch:
	cpsid if

	push {r4-r11}
	push {lr}
	mrs r0, psp
	push {r0}

	sub r1, sp, #4
	push {r1}                /* ctx->savesp = sp */

	mrs r0, ipsr
	cmp r0, #14
	bne _intd0

	ldr r5, =_cpu_nctx
	ldr r6, [r5]
	cmp r6, #0
	beq _intd0
	mov sp, r6
	mov r6, #0
	str r6, [r5]
	b _intd1

_intd0:
	bl interrupts_dispatch
	ldr sp, [sp]             /* sp = ctx->savesp */
_intd1:
	add sp, sp, #4

	pop {r0}
	msr psp, r0
	pop {lr}

	tst lr, #4
	ite eq
	moveq r4, #0
	movne r4, #1
	msr control, r4

	pop {r4-r11}

	cpsie if
	bx lr
.size _interrupts_dispatch, .-_interrupts_dispatch


.globl hal_cpuReschedule
.type hal_cpuReschedule, %function
hal_cpuReschedule:
	push {r0, lr}
	bl _hal_invokePendSV
	pop {r1, lr}
	cmp r1, #NULL
	it eq
	beq hal_cpuReschedule0
	add r1, r1, #12
	ldrexb r2, [r1]
	add r2, r2, #1
	dmb
	strexb r3, r2, [r1]
	ldrb r2, [r1, #1]
	msr primask, r2
hal_cpuReschedule0:
	isb
	dmb
	bx lr
.size hal_cpuReschedule, .-hal_cpuReschedule
