/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for Cortex-M33 (ARMv8) architecture
 *
 * Copyright 2012, 2016-2017, 2022 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

#define ADDR_SCB 0xe000ed00u

.syntax unified
.cpu cortex-m33

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
.word _exceptions_dispatch /* SecureFault */
.word 0
.word 0
.word 0
.word _syscall_dispatch    /* SVC */
.word _exceptions_dispatch /* Debug */
.word 0
.word _interrupts_dispatch /* PendSV */
.word _interrupts_dispatch /* Systick */

.rept 67                    /* Max number of ext interrupts - last peripheral id + 1 */
.word _interrupts_dispatch
.endr
.size _init_vectors, .-_init_vectors


.thumb
.thumb_func

.globl _start
.type _start, %function

_start:
	cpsid if
	isb

	/* Point to syspage */
	ldr r8, =syspage
	str r9, [r8]

	/* Init vector table pointer */
	ldr r0, =0xe000ed08u
	ldr r1, =_init_vectors
	str r1, [r0]
	isb
	dmb

	/* Init MSP to a first value in _init_vectors (.bss end + 1024 + 256) */
	ldr r0, [r1]
	bic r0, 7
	msr msp, r0
	isb

	bl _nrf91_init

	bl main
.size _start, .-_start
.ltorg

.globl _syscall_dispatch
.type _syscall_dispatch, %function
.type _syscallend, %function

_syscall_dispatch:
	mov r0, #0
	msr control, r0
	isb

	mov r0, sp

	/* Prepare context on kernel stack */
	sub sp, sp, #((14 * 4) + (8 * 4))
	str sp, [sp]

	add r1, sp, #12
	mov r12, #0
	stmia r1!, {r4-r12}
	str r0, [r1], #8 /* msp */
	mov r10, r1 /* addr to store hw-saved ctx */

	mrs r0, psp
	str r0, [sp, #8]

	/* Load hardware saved registers from user stack */
	ldmia r0!, {r1-r8}

	/* Fix PC LSB not being set */
	orr r7, r7, #1

	/* Store hardware saved registers on kernel stack */
	add r10, r10, #(5 * 4)
	stm r10, {r6-r8} /* lr, pc, psr */

	/* Copy arguments back to the user stack */
	stmdb r0!, {r1-r4}
	mov r1, r0
	mov r2, sp
	ldrb r0, [r7, #-3]

	/* Prepare pseudo context */
	mov r7, #0x01000000
	ldr r6, =syscalls_dispatch
	ldr r5, =_syscallend
	stmdb sp!, {r0-r7} /* PSR, PC, LR, R12, R3, R2, R1, R0 */

	/* Exit handler mode to kernel thread mode */
	ldr lr,=RET_THREAD_MSP
	bx lr

_syscallend:
	/* return value in r0 */
	ldr lr, [sp, #8] /* psp */
	add lr, lr, #(8 * 4)
	msr psp, lr
	isb

	add r10, sp, #((14 * 4) + (5 * 4))
	ldm r10, {r2,r3} /* lr, pc */

	add lr, sp, #12
	ldmia lr!, {r4-r11}
	add lr, lr, #(11 * 4)
	mov sp, lr

	/* Switch stacks */
	mov r1, #3
	msr control, r1
	isb
	dsb

	mov lr, r2
	mov pc, r3
.size _syscall_dispatch, .-_syscall_dispatch
.ltorg

.globl _exceptions_dispatch
.type _exceptions_dispatch, %function

_exceptions_dispatch:
	cpsid if
	isb

	mov r0, sp /* save MSP to be stored in cpu_context_t */
	tst lr, #(1 << 2)
	ite ne
	subne sp, sp, #(8 * 4) /* If coming from userspace, make room on the stack for hardware-stored context */
	addeq r0, r0, #(8 * 4) /* If coming from kernel, calculate msp as it was before the exception  */

	str r0, [sp, #-8]! /* Store msp, skip over pad0 */

	mrs r0, ipsr
	mrs r3, psp
	sub r1, sp, #48
	stmdb sp!, {r1-r11, lr}

	b exceptions_dispatch
.size _exceptions_dispatch, .-_exceptions_dispatch
.ltorg

.globl _interrupts_dispatch
.type _interrupts_dispatch, %function
_interrupts_dispatch:
	mov r0, #0
	msr control, r0
	isb

	mov r0, sp
	and r12, lr, #(1 << 2)
	cmp r12, #0
	it ne
	subne sp, sp, #(8 * 4) /* space for hw-saved ctx */
	str r0, [sp, #-8]!

	mrs r0, ipsr
	mrs r3, psp
	sub r1, sp, #48
	stmdb sp!, {r1-r11, lr}

	/* if we came from userspace, copy hw-saved regs to kstack
	 * in case of signal handling
	 */

	beq _intd0 /* cmp r12, #0 */

	/* psp in r3 - load hw-saved regs */
	ldm r3, {r4-r11}
	add r12, sp, #(14 * 4)
	stm r12, {r4-r11}

_intd0:
	bl interrupts_dispatch
	ldr r1, [sp]
	mov sp, r1
	isb
_intd1:
	ldr lr, [sp, #(11 * 4)]

	and r12, lr, #(1 << 2)
	cmp r12, #0
	beq _intd2

	/* userspace return - restore registers in case of signal handling */
	ldr r3, [sp, #(2 * 4)] /* psp */
	add r1, sp, #(14 * 4)
	ldm r1, {r4-r11}
	stm r3, {r4-r11}

_intd2:
	ldmia sp!, {r1-r11}
	ldr r0, [sp, #4]
	mov sp, r0

	msr psp, r3
	isb

	/* Check if we're returning to userspace */
	and r1, lr, #4
	ror r1, r1, #2
	msr control, r1
	isb

	bx lr
.size _interrupts_dispatch, .-_interrupts_dispatch
.ltorg


.globl _hal_invokePendSV
.type _hal_invokePendSV, %function
_hal_invokePendSV:
	mov r1, #(1 << 28)
	ldr r2, =ADDR_SCB
	str r1, [r2, #4]
	bx lr
.size _hal_invokePendSV, .-_hal_invokePendSV
.ltorg

.globl hal_cpuReschedule
.type hal_cpuReschedule, %function
hal_cpuReschedule:
	push {r0-r2, lr}
	eor r0, r0, r0 /* default return value */
	bl _hal_invokePendSV
	pop {r1-r3, lr}
	cmp r1, #NULL
	beq hal_cpuReschedule0
	push {r3-r4}
	add r1, r1, #12

spinlock:
	ldrexb r3, [r1]
	add r3, r3, #1
	dmb
	strexb r4, r3, [r1]
	cmp r4, #0
	bne spinlock
	pop {r3-r4}
hal_cpuReschedule0:
	cpsie if
	isb
	dmb
	bx lr
.size hal_cpuReschedule, .-hal_cpuReschedule
.ltorg


.globl hal_jmp /* void hal_jmp(void *f, void *kstack, void *ustack, int kargc, const arg_t *kargs) */
.type hal_jmp, %function
hal_jmp:
	cpsid if
	isb

	cmp r2, #NULL
	bne hal_jmp_user

	/* kargs has been passed on the stack */
	ldr r5, [sp]
	mov r4, r0
	mov sp, r1
	isb
	subs r3, #1
	bmi 1f
	ldr r0, [r5]
	subs r3, #1
	bmi 1f
	ldr r1, [r5, #4]
	subs r3, #1
	bmi 1f
	ldr r2, [r5, #8]
	subs r3, #1
	bmi 1f
	ldr r3, [r5, #12]
1:
	cpsie if
	isb
	dmb
	bx r4

hal_jmp_user:
	cpsid if
	isb
	msr msp, r1
	isb
	msr psp, r2
	cpsie if
	isb
	mov r1, USERCONTROL
	msr control, r1
	isb
	dmb
	bx r0

.size hal_jmp, .-hal_jmp
.ltorg

.globl hal_exceptionJump /* void hal_exceptionJump(unsigned int n, exc_context_t *ctx, void (*handler)(unsigned int, exc_context_t *)) */
.type hal_exceptionJump, %function
hal_exceptionJump:
	push {r4-r11, lr}
	ldr r7, =0x01000000
	mov r6, r2
	ldr r5, =1f
	orr r5, #1
	push {r0-r7}

	mov lr, #0
	msr control, lr
	mov lr, 0xfffffff9
	cpsie if
	dsb
	bx lr

1:	pop {r4-r11, lr}
	bx lr

.size hal_exceptionJump, .-hal_exceptionJump
.ltorg
