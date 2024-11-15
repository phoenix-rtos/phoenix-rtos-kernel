/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception stubs
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

#define CTXPUSHL 44 /* 11 + 5 + 28 */

.text

.align 4, 0x90
exception_pushContext:
	/* Save error code */
	xchgl 4(%esp), %eax
	movl %eax, -(4 * CTXPUSHL - 4)(%esp)
	/* Save return address for ret */
	movl (%esp), %eax
	movl %eax, -(4 * CTXPUSHL)(%esp)
	subl $FPU_CONTEXT_SIZE - 4, %esp
	/* Check TS flag in CR0 register */
	movl %cr0, %eax
	andl $CR0_TS_BIT, %eax
	xchgl FPU_CONTEXT_SIZE(%esp), %eax
	jnz .exception_pushRegisters
	/* Save FPU context */
	fnsave (%esp)
.exception_pushRegisters:
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
	pushl %esp

	movl %dr7, %eax
	pushl %eax
	movl %dr6, %eax
	pushl %eax
	movl %dr3, %eax
	pushl %eax
	movl %dr2, %eax
	pushl %eax
	movl %dr1, %eax
	pushl %eax
	movl %dr0, %eax
	pushl %eax
	subl $8, %esp
	ret
.size exception_pushContext, .-exception_pushContext

.align 4, 0x90
exception_popContext:
	addl $4, %esp /* skip err */
	popl %eax
	movl %eax, %dr0
	popl %eax
	movl %eax, %dr1
	popl %eax
	movl %eax, %dr2
	popl %eax
	movl %eax, %dr3
	popl %eax
	movl %eax, %dr6
	popl %eax
	movl %eax, %dr7
	addl $4, %esp /* skip savesp */

	popl %edi
	popl %esi
	popl %ebp
	popl %edx
	popl %ecx
	popl %ebx
	/* FPU context */
	testl $CR0_TS_BIT, (12 + FPU_CONTEXT_SIZE)(%esp)
	jz .Linterrupts_popFPU
	movl %cr0, %eax
	orl $CR0_TS_BIT, %eax
	movl %eax, %cr0
	jmp .Linterrupts_popFPUend
.Linterrupts_popFPU:
	clts
	frstor 12(%esp)
.Linterrupts_popFPUend:
	popl %eax
	popw %gs
	popw %fs
	popw %es
	popw %ds
	/* Skip over FPU context and cr0Bits. */
	addl $FPU_CONTEXT_SIZE + 4, %esp
	iret
.size exception_popContext, .-exception_popContext

/* Used for FPU lazy context stacking */
.globl exceptions_exc7_handler
.align 4, 0x90
.type exceptions_exc7_handler, @function
exceptions_exc7_handler:
	/* Clear task-switched flag */
	clts
	movl 8(%esp), %eax /* eax := ctx */
	/* ctx->cr0Bits &= ~CR0_TS_BIT */
	movl 176(%eax), %ecx /* ecx := ctx->cr0Bits */
	andl $~CR0_TS_BIT, %ecx /* ecx := ctx->cr0Bits & ~CR0_TS_BIT */
	movl %ecx, 176(%eax)
	/* Init FPU */
	fninit
	fnsave 68(%eax) /* Set ctx->fpuContext */
	ret

.size exceptions_exc7_handler, .-exceptions_exc7_handler

/* Exception stub function definition */
#define EXCDEF(sym) \
.globl sym; \
.align 4, 0x90; \
.type sym, @function; \
sym:


/* Exception handling macro */
#define EXCSTUB(exc)\
	cli;\
	call exception_pushContext;\
	movl $SEL_KDATA, %eax;\
	movw %ax, %ds;\
	movw %ax, %es;\
	movw %ax, %fs;\
	movw %ax, %gs;\
	;\
	/* Call exception handler */ ;\
	leal 0(%esp), %eax;\
	pushl %eax;\
	movl exceptions + (exc) * 4, %eax;\
	pushl $exc;\
	call *%eax;\
	addl $8, %esp;\
	/* cpu_context_t */;\
	movl %esp, %eax;\
	addl $28, %eax;\
	pushl %eax;\
	call hal_cpuSupervisorMode;\
	addl $4, %esp;\
	testl %eax, %eax;\
	jnz exception_popContext;\
	/* return value */ ;\
	movl 56(%esp), %eax;\
	pushl %eax;\
	call threads_setupUserReturn;\
	addl $4, %esp;\
	;\
	jmp exception_popContext


/* Exception stubs */

EXCDEF(_exceptions_exc0)
	pushl $0;
	EXCSTUB(0);
.size _exceptions_exc0, .-_exceptions_exc0

EXCDEF(_exceptions_exc1)
	pushl $0;
	EXCSTUB(1);
.size _exceptions_exc1, .-_exceptions_exc1

EXCDEF(_exceptions_exc2)
	pushl $0;
	EXCSTUB(2);
.size _exceptions_exc2, .-_exceptions_exc2

EXCDEF(_exceptions_exc3)
	pushl $0
	EXCSTUB(3);
.size _exceptions_exc3, .-_exceptions_exc3

EXCDEF(_exceptions_exc4)
	pushl $0;
	EXCSTUB(4);
.size _exceptions_exc4, .-_exceptions_exc4

EXCDEF(_exceptions_exc5);
	pushl $0
	EXCSTUB(5);
.size _exceptions_exc5, .-_exceptions_exc5

EXCDEF(_exceptions_exc6);
	pushl $0
	EXCSTUB(6);
.size _exceptions_exc6, .-_exceptions_exc6

EXCDEF(_exceptions_exc7);
	pushl $0
	EXCSTUB(7);
.size _exceptions_exc7, .-_exceptions_exc7

EXCDEF(_exceptions_exc8);
	EXCSTUB(8);
.size _exceptions_exc8, .-_exceptions_exc8

EXCDEF(_exceptions_exc9);
	pushl $0
	EXCSTUB(9);
.size _exceptions_exc9, .-_exceptions_exc9

EXCDEF(_exceptions_exc10);
	EXCSTUB(10);
.size _exceptions_exc10, .-_exceptions_exc10

EXCDEF(_exceptions_exc11);
	EXCSTUB(11);
.size _exceptions_exc11, .-_exceptions_exc11

EXCDEF(_exceptions_exc12);
	EXCSTUB(12);
.size _exceptions_exc12, .-_exceptions_exc12

EXCDEF(_exceptions_exc13);
	EXCSTUB(13);
.size _exceptions_exc13, .-_exceptions_exc13

EXCDEF(_exceptions_exc14);
	EXCSTUB(14);
.size _exceptions_exc14, .-_exceptions_exc14

EXCDEF(_exceptions_exc15);
	pushl $0
	EXCSTUB(15);
.size _exceptions_exc15, .-_exceptions_exc15

EXCDEF(_exceptions_exc16);
	pushl $0
	EXCSTUB(16);
.size _exceptions_exc16, .-_exceptions_exc16

EXCDEF(_exceptions_exc17);
	EXCSTUB(17);
.size _exceptions_exc17, .-_exceptions_exc17

EXCDEF(_exceptions_exc18);
	pushl $0
	EXCSTUB(18);
.size _exceptions_exc18, .-_exceptions_exc18

EXCDEF(_exceptions_exc19);
	pushl $0
	EXCSTUB(19);
.size _exceptions_exc19, .-_exceptions_exc19

EXCDEF(_exceptions_exc20);
	pushl $0
	EXCSTUB(20);
.size _exceptions_exc20, .-_exceptions_exc20

EXCDEF(_exceptions_exc21);
	EXCSTUB(21);
.size _exceptions_exc21, .-_exceptions_exc21

/* Marked as reserved on ia32 without error code data, set according to amd64 spec. */
EXCDEF(_exceptions_exc22);
	pushl $0
	EXCSTUB(22);
.size _exceptions_exc22, .-_exceptions_exc22

EXCDEF(_exceptions_exc23);
	pushl $0
	EXCSTUB(23);
.size _exceptions_exc23, .-_exceptions_exc23

EXCDEF(_exceptions_exc24);
	pushl $0
	EXCSTUB(24);
.size _exceptions_exc24, .-_exceptions_exc24

EXCDEF(_exceptions_exc25);
	pushl $0
	EXCSTUB(25);
.size _exceptions_exc25, .-_exceptions_exc25

EXCDEF(_exceptions_exc26);
	pushl $0
	EXCSTUB(26);
.size _exceptions_exc26, .-_exceptions_exc26

EXCDEF(_exceptions_exc27);
	pushl $0
	EXCSTUB(27);
.size _exceptions_exc27, .-_exceptions_exc27

EXCDEF(_exceptions_exc28);
	pushl $0
	EXCSTUB(28);
.size _exceptions_exc28, .-_exceptions_exc28

EXCDEF(_exceptions_exc29);
	EXCSTUB(29);
.size _exceptions_exc29, .-_exceptions_exc29

EXCDEF(_exceptions_exc30);
	EXCSTUB(30);
.size _exceptions_exc30, .-_exceptions_exc30

EXCDEF(_exceptions_exc31);
	pushl $0
	EXCSTUB(31);
.size _exceptions_exc31, .-_exceptions_exc31
