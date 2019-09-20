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


.extern _cpu_nctx
.extern syscalls
.extern syspage


.section .init, "x"

.globl _init_vectors
.type _init_vectors, %object
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
.word _pendsv              /* PendSV */
.word _interrupts_dispatch /* Systick */

.rept 68
.word _interrupts_dispatch
.endr
.size _init_vectors, .-_init_vectors

.org _init_vectors + 512

_syspage:
.word 0x20000000                        /* pbegin */
.word 0x20000000 + (1024 * RAM_SIZE)    /* pend */
.word 0x00000000                        /* *arg */
.word 0x00000000                        /* progssz */

.org _syspage + 1024

.thumb
.thumb_func

.globl _start
.type _start, %function

_start:
	cpsid if

	/* Init vector table pointer */
	ldr r0, =0xe000ed08
	ldr r1, =_init_vectors
	str r1, [r0]
	bl _stm32_init

	/* Pass syspage to main */
	ldr r0, =_syspage
	ldr r1, =syspage
	str r0, [r1]
	bl main
.size _start, .-_start
.ltorg

.globl _syscall_dispatch
.type _syscall_dispatch, %function
.type _syscallend, %function

_syscall_dispatch:
	stmdb sp!, {r4-r8}
	mrs r0, psp

	/* Store hardware saved registers on kernel stack */
	ldmia r0!, {r1-r8}

	/* Fix PC LSB not being set */
	orr r7, r7, #1

	stmdb sp!, {r1-r8}
	push {r0}

	/* Copy arguments back to the user stack */
	stmdb r0!, {r1-r4}

	/* Get syscall number and its function pointer */
	ldr r1, =syscalls
	ldrb r2, [r7, #-3]
	ldr r6, [r1, r2, lsl #2]

	/* Prepare pseudo context */
	mov r7, #0x01000000
	ldr r5, =_syscallend
	stmdb sp!, {r0-r7} /* PSR, PC, LR, R12, R3, R2, R1, R0 */

	/* Exit handler mode to kernel thread mode */
	mov lr, #0
	msr control, lr
	mov lr, #0xfffffff9
	cpsie if
	bx lr

_syscallend:
	pop {lr}

	ldmia sp!, {r1-r8}
	mov r1, r0 /* Put result to user's r0 */
	stmdb lr!, {r1-r8}

	ldmia sp!, {r4-r8}

	/* Switch stacks */
	mov r0, #3
	msr control, r0

	ldmia sp!, {r0-r3, r12, lr}
	ldr pc, [sp], #8
.size _syscall_dispatch, .-_syscall_dispatch
.ltorg

.globl _exceptions_dispatch
.type _exceptions_dispatch, %function

_exceptions_dispatch:
	cpsid if

	mrs r0, psp
	stmdb sp!, {r0, r4-r11, lr}

	mrs r0, ipsr
	mov r1, sp
	b exceptions_dispatch
.size _exceptions_dispatch, .-_exceptions_dispatch
.ltorg

.globl _pendsv
.type _pendsv, %function
_pendsv:
	cpsid if

	mrs r0, ipsr
	mrs r2, psp
	sub r1, sp, #44
	stmdb sp!, {r1-r2, r4-r11, lr}

	ldr r5, =_cpu_nctx
	ldr r6, [r5]
	cmp r6, #0
	beq _intd0
	mov sp, r6
	mov r6, #0
	str r6, [r5]
	b _intd1
.size _pendsv, .-_pendsv
.ltorg

.globl _interrupts_dispatch
.type _interrupts_dispatch, %function
_interrupts_dispatch:
	cpsid if

	mrs r0, ipsr
	mrs r2, psp
	sub r1, sp, #44
	stmdb sp!, {r1-r2, r4-r11, lr}

_intd0:
	bl interrupts_dispatch
	ldr sp, [sp]
_intd1:
	ldmia sp!, {r0-r1, r4-r11, lr}

	msr psp, r1

	/* Check if we're returning to userspace */
	and r1, lr, #4
	ror r1, r1, #2
	msr control, r1

	cpsie if
	bx lr
.size _interrupts_dispatch, .-_interrupts_dispatch
.ltorg

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
.ltorg

