/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for Cortex-M7 (ARMv7) architecture
 *
 * Copyright 2012, 2016-2017, 2020 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

.syntax unified
.cpu cortex-m7

.extern syscalls
.extern syspage

#define FLEXSPI2_BASE 0x70000000

#define ADDR_ITCM 0x00000000
#define ADDR_DTCM 0x20000000
#define ADDR_SCB  0xe000ed00


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
.word _interrupts_dispatch /* PendSV */
.word _interrupts_dispatch /* Systick */

.rept 240
.word _interrupts_dispatch
.endr
.size _init_vectors, .-_init_vectors

.thumb
.thumb_func

.globl _start
.type _start, %function
_start:
	cpsid if

	/* Point to syspage */
	ldr r8, =syspage
	str r9, [r8]

	/* Enable FPU */
	ldr r0, =0xe000ed88
	ldr r1, [r0]
	orr r1, r1, #(0xf << 20)
	str r1, [r0]
	isb
	dmb

	/* Enable FPU ctx */
	mrs r0, control
	orr r0, r0, #(1 << 2)
	msr control, r0
	isb

	/* Init vector table and stack pointer */
	ldr r0, =0xe000ed08
	ldr r1, =_init_vectors
	str r1, [r0]
	isb
	dmb
	ldr r0, [r1]
	bic r0, 7
	msr msp, r0
	isb
	bl _imxrt_init

	ldr r8, =main
	bx r8
.size _start, .-_start
.ltorg


.globl _syscall_dispatch
.type _syscall_dispatch, %function
.type _syscallend, %function

_syscall_dispatch:
	mov r0, sp
	sub sp, sp, #(26 * 4) /* space for hw-saved ctx */

	vstmdb sp!, {s16-s31}
	str r0, [sp, #-8]!
	mov r12, #0
	stmdb sp!, {r4-r12}

	add r10, sp, #((16 * 4) + (11 * 4)) /* addr to store hw-saved ctx */

	mrs r0, psp
	mov r12, r0 /* save original psp */

	ldmia r0!, {r1-r8}
	orr r7, r7, #1

	/* r0-r3, r12, lr, pc, psr */
	stmia r10!, {r1-r8}

	/* s0-s15, fpscr */
	vldmia r0!, {s0-s16}
	vstmia r10!, {s0-s16}

	add r0, r0, #4 /* skip padding */

	sub sp, sp, #12
	str sp, [sp]     /* savesp */

	ldr r6, =0xe000ef38
	ldr r2, [r6]     /* fpuctx */
	str r2, [sp, #4]

	str r12, [sp, #8] /* ustack */

	/* At this point full context is saved on kernel stack */
	add r2, sp, #(38 * 4)
	str sp, [r6]
	isb
	dmb

	/* Load saved registers again */
	add r10, sp, #(30 * 4)
	ldm r10, {r1-r8}

	stmdb r0!, {r1-r4}

	sub sp, sp, #(18 * 4)

	mov r1, r0
	ldrb r0, [r7, #-3]
	ldr r6, =syscalls_dispatch

	mov r7, #0x01000000
	ldr r5, =_syscallend
	stmdb sp!, {r0-r7}

	mov lr, #(1 << 2)
	msr control, lr
	mov lr, #0xffffffe9
	isb
	dsb

	bx lr

_syscallend:
	/* return value in r0 */
	ldr lr, [sp, #8] /* psp */
	add lr, lr, #(26 * 4)
	msr psp, lr
	isb

	add r10, sp, #((30 * 4) + (5 * 4))
	ldm r10, {r2,r3} /* lr, pc */

	add lr, sp, #12
	ldmia lr!, {r4-r11}
	add lr, lr, #12
	mov sp, lr
	vldmia sp!, {s16-s31}
	add sp, sp, #(8 * 4)
	vldmia sp!, {s0-s15}
	ldr r1, [sp], #8 /* fpscr */
	vmsr fpscr, r1

	/* Switch stacks */
	mov r1, #7
	msr control, r1
	isb

	mov lr, r2
	mov pc, r3
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


.globl _interrupts_dispatch
.type _interrupts_dispatch, %function
_interrupts_dispatch:
	mov r0, sp

	tst lr, #(1 << 2)
	it ne
	subne sp, sp, #(26 * 4) /* space for hw-saved ctx */

	vstmdb sp!, {s16-s31}
	str r0, [sp, #-8]!
	isb
	mrs r0, ipsr
	mrs r3, psp
	sub r1, sp, #48
	ldr r2, =0xe000ef38
	ldr r2, [r2]
	stmdb sp!, {r1-r11, lr}

	/* if we came from userspace, copy hw-saved regs to kstack
	 * in case of signal handling
	 */

	beq _intd0 /* tst lr, #(1 << 2) */

	/* psp in r3 - load hw-saved regs */
	vldm r3, {s0-s24}
	add r12, sp, #(30 * 4)
	vstm r12, {s0-s24}

_intd0:
	bl interrupts_dispatch
	isb
	dmb
	ldr r1, [sp]
	mov sp, r1
	isb
_intd1:
	ldmia sp!, {r1-r11, lr}
	ldr r0, [sp], #8

	tst lr, #(1 << 2)
	beq _intd2

	/* userspace return - restore registers in case of signal handling */
	add r12, sp, #(16 * 4)
	vldm r12, {s0-s24}
	vstm r3, {s0-s24}
_intd2:
	vldmia sp!, {s16-s31}
	isb

	mov sp, r0

	ldr r1, =0xe000ef38
	str r2, [r1]
	isb
	dmb

	msr psp, r3
	isb

	/* Check if we're returning to userspace */
	and r1, lr, #4
	ror r1, r1, #2
	orr r1, r1, #(1 << 2)
	msr control, r1
	isb

	dsb
	bx lr
.size _interrupts_dispatch, .-_interrupts_dispatch
.ltorg


.globl _hal_invokePendSV
.type _hal_invokePendSV, %function
_hal_invokePendSV:
	mov r1, #(1 << 28)
	ldr r2, =ADDR_SCB
	str r1, [r2, #4]
	isb
	dmb
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

	push {r0-r3}
	pop {r4-r8} /* kargs has been passed on the stack */

	cmp r6, #NULL
	bne hal_jmp_user

	mov sp, r5
	isb
	subs r7, #1
	bmi 1f
	ldr r0, [r8]
	subs r7, #1
	bmi 1f
	ldr r1, [r8, #4]
	subs r7, #1
	bmi 1f
	ldr r2, [r8, #8]
	subs r7, #1
	bmi 1f
	ldr r3, [r8, #12]
1:
	cpsie if
	isb
	dmb
	bx r4

hal_jmp_user:
	cpsid if
	isb
	msr msp, r5
	isb
	msr psp, r6
	cpsie if
	isb
	mov r5, USERCONTROL
	msr control, r5
	isb
	dmb
	bx r4

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
	mov lr, 0xffffffe9
	cpsie if
	dsb
	bx lr

1:	pop {r4-r11, lr}
	bx lr

.size hal_exceptionJump, .-hal_exceptionJump
.ltorg
