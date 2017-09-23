/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception stubs
 *
 * Copyright 2012, 2016-2018 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Michał Mirosław
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "cpu.h"


.text

/* Exceptions that push error code (8, 10-14, 17, 30) */
.set pushed_mask, 0x40027d00


/* Exception handler stubs */
.macro EXCSTUB i

.globl _exceptions_exc\i
.align 2, 0x90
.type _exceptions_exc\i, @function
_exceptions_exc\i:

.ifeq (pushed_mask >> \i) & 1
	pushl $0
.endif
	pushl $\i
	jmp _exception_handler

.size _exceptions_exc\i, .-_exceptions_exc\i

.endm


/* Exception handler stubs */
.irp i, 31, 30, 20, 19, 18, 16, 15, 9, 7, 6, 5, 4, 3, 2, 1, 0, 17, 11, 10, 8, 12, 13, 14
EXCSTUB \i
.endr


/* Common exception stub body */
.globl _exception_handler
.align 2, 0x90
.type _exception_handler, @function
_exception_handler:
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

	movl 36(%esp), %ebx

	movl %dr5, %eax
	pushl %eax
	movl %dr4, %eax
	pushl %eax
	movl %dr3, %eax
	pushl %eax
	movl %dr2, %eax
	pushl %eax
	movl %dr1, %eax
	pushl %eax
	movl %dr0, %eax
	pushl %eax

	movl $SEL_KDATA, %eax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs

	/* Call exception handler */
	movl exceptions(,%ebx,4), %eax
	pushl %esp
	pushl %ebx
	call *%eax
	addl $8, %esp

	popl %eax
	movl %eax, %dr0
	popl %eax
	movl %eax, %dr1
	popl %eax
	movl %eax, %dr2
	popl %eax
	movl %eax, %dr3
	popl %eax
	movl %eax, %dr4
	popl %eax
	movl %eax, %dr5

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
	addl $8, %esp
	iret
.size _exception_handler, .-_exception_handler


/* Undefined exceptions; kept here to have shorter jumps */
.irp i, 29, 28, 27, 26, 25, 24, 23, 22, 21
EXCSTUB \i
.endr
