/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt stubs
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author; Pawel Pisarczyk, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "cpu.h"

.text

.macro SAVE_ALL
//	LOCAL _restore_kernel_tpsp
//	LOCAL _save_context

	/*
	 * If coming from userspace, preserve the user thread pointer and load
	 * the kernel thread pointer.  If we came from the kernel, sscratch
	 * will contain 0, and we should continue on the current TP.
	 */
/*	csrrw tp, sscratch, tp
	bnez tp, _save_context

_restore_kernel_tpsp:
	csrr tp, sscratch
	REG_S sp, TASK_TI_KERNEL_SP(tp)
_save_context:*/
	sd sp, (sp)
//	addi sp, sp, -(PT_SIZE_ON_STACK)
	sd x1, (sp)
	sd x3, 8(sp)
	sd x5, 16(sp)
	sd x6, 24(sp)
	sd x7, 32(sp)
	sd x8, 40(sp)
	sd x9, 48(sp)
	sd x10, 56(sp)
	sd x11, 64(sp)
	sd x12, 72(sp)
	sd x13, 80(sp)
	sd x14, 88(sp)
	sd x15, 96(sp)
	sd x16, 104(sp)
	sd x17, 112(sp)
	sd x18, 120(sp)
	sd x19, 128(sp)
	sd x20, 136(sp)
	sd x21, 144(sp)
	sd x22, 152(sp)
	sd x23, 160(sp)
	sd x24, 168(sp)
	sd x25, 176(sp)
	sd x26, 184(sp)
	sd x27, 192(sp)
	sd x28, 200(sp)
	sd x29, 208(sp)
	sd x30, 216(sp)
	sd x31, 224(sp)

	/*
	 * Disable user-mode memory access as it should only be set in the
	 * actual user copy routines.
	 *
	 * Disable the FPU to detect illegal usage of floating point in kernel
	 * space.
	 */
	li t0, SR_SUM | SR_FS

	ld s0, TASK_TI_USER_SP(tp)
	csrrc s1, sstatus, t0
	csrr s2, sepc
	csrr s3, sbadaddr
	csrr s4, scause
	csrr s5, sscratch
	sd s0, 232(sp)
	sd s1, 240(sp)
	sd s2, 248(sp)
	sd s3, 256(sp)
	sd s4, 264(sp)
	sd s5, 272(sp)
.endm


#if 0
/*
 * Prepares to return from a system call or exception by restoring all
 * registers from the stack.
 */
.macro RESTORE_ALL
	REG_L a0, PT_SSTATUS(sp)
	REG_L a2, PT_SEPC(sp)
	csrw sstatus, a0
	csrw sepc, a2

	REG_L x1,  PT_RA(sp)
	REG_L x3,  PT_GP(sp)
	REG_L x4,  PT_TP(sp)
	REG_L x5,  PT_T0(sp)
	REG_L x6,  PT_T1(sp)
	REG_L x7,  PT_T2(sp)
	REG_L x8,  PT_S0(sp)
	REG_L x9,  PT_S1(sp)
	REG_L x10, PT_A0(sp)
	REG_L x11, PT_A1(sp)
	REG_L x12, PT_A2(sp)
	REG_L x13, PT_A3(sp)
	REG_L x14, PT_A4(sp)
	REG_L x15, PT_A5(sp)
	REG_L x16, PT_A6(sp)
	REG_L x17, PT_A7(sp)
	REG_L x18, PT_S2(sp)
	REG_L x19, PT_S3(sp)
	REG_L x20, PT_S4(sp)
	REG_L x21, PT_S5(sp)
	REG_L x22, PT_S6(sp)
	REG_L x23, PT_S7(sp)
	REG_L x24, PT_S8(sp)
	REG_L x25, PT_S9(sp)
	REG_L x26, PT_S10(sp)
	REG_L x27, PT_S11(sp)
	REG_L x28, PT_T3(sp)
	REG_L x29, PT_T4(sp)
	REG_L x30, PT_T5(sp)
	REG_L x31, PT_T6(sp)

	REG_L x2,  PT_SP(sp)
.endm




.global interrupts_pushContext
interrupts_pushContext:
	xchgl (%esp), %edx
	movl %edx, -(4 * CTXPUSHL)(%esp)
	popl %edx
	pushw %ds
	pushw %es
	pushw %fs
	pushw %gs
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %ebp
	pushl %esi
	pushl %edi
#ifndef NDEBUG
	movl %dr3, %edx
	pushl %edx
	movl %dr2, %edx
	pushl %edx
	movl %dr1, %edx
	pushl %edx
	movl %dr0, %edx
	pushl %edx
#endif
	pushl %esp
	subl $4, %esp
	ret
.size interrupts_pushContext, .-interrupts_pushContext


.global interrupts_popContext
interrupts_popContext:
	popl %esp
	leal -4(%esp), %eax
	call interrupts_checkSignal
#ifndef NDEBUG
	popl %edx
	movl %edx, %dr0
	popl %edx
	movl %edx, %dr1
	popl %edx
	movl %edx, %dr2
	popl %edx
	movl %edx, %dr3
#endif
	popl %edi
	popl %esi
	popl %ebp
	popl %edx
	popl %ecx
	popl %ebx
	popl %eax
	popw %gs
	popw %fs
	popw %es
	popw %ds
	iret
.size interrupts_popContext, .-interrupts_popContext


#define INTERRUPT(name, intr, func) \
.globl name; \
.type name, @function; \
.align 4, 0x90; \
name:; \
	call interrupts_pushContext; \
	movl $SEL_KDATA, %eax; \
	movw %ax, %ds; \
	movw %ax, %es; \
	movw %ax, %fs; \
	movw %ax, %gs; \
	pushl %esp; \
	pushl $intr; \
	call func; \
	addl $8, %esp; \
	jmp interrupts_popContext; \
.size name, .-name


INTERRUPT(_interrupts_irq0, 0, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq1, 1, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq2, 2, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq3, 3, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq4, 4, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq5, 5, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq6, 6, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq7, 7, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq8, 8, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq9, 9, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq10, 10, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq11, 11, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq12, 12, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq13, 13, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq14, 14, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq15, 15, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_unexpected, 255, _interrupts_unexpected)


.globl _interrupts_syscall
.type _interrupts_syscall, @function
.align 4, 0x90
_interrupts_syscall:
	call interrupts_pushContext
	movl $SEL_KDATA, %edx
	movw %dx, %ds
	movw %dx, %es
	movl (4 * CTXPUSHL + 12)(%esp), %edx
	pushl %edx
	pushl %eax
	call syscalls_dispatch
	addl $8, %esp
	movl %eax, (4 * CTXPUSHL - 12)(%esp)
	jmp interrupts_popContext
.size _interrupts_syscall, .-_interrupts_syscall
#endif