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
	.cfi_startproc
	.cfi_signal_frame
	.cfi_def_cfa_offset 12
	.cfi_escape /* esp = CFA + 8; if (*CFA & 3 != 0) esp = *esp */ \
		0x16, 4, 13,	/* DW_CFA_val_expression esp, block: */ \
		0x12,			/* DW_OP_dup			| cfa, cfa */ \
		0x23, 8,		/* DW_OP_plus_uconst 8		| esp=cfa+8, cfa */ \
		0x16,			/* DW_OP_swap			| cfa, esp */ \
		0x06,			/* DW_OP_deref			| cs, esp */ \
		0x33,			/* DW_OP_lit3			| 3, cs, esp */ \
		0x1a,			/* DW_OP_and			| cs & 3, esp */ \
		0x30,			/* DW_OP_lit0			| 0, cs & 3, esp */ \
		0x29,			/* DW_OP_eq			| cs & 3 == 0, esp */ \
		0x28, 1, 0,		/* DW_OP_bra +1			| esp */ \
		0x06			/* DW_OP_deref			| *esp */
	pushw %ds
	.cfi_adjust_cfa_offset 2
	pushw %es
	.cfi_adjust_cfa_offset 2
	pushw %fs
	.cfi_adjust_cfa_offset 2
	pushw %gs
	.cfi_adjust_cfa_offset 2
	pushl %eax
	.cfi_adjust_cfa_offset 4
	.cfi_rel_offset eax, 0
	pushl %ebx
	.cfi_adjust_cfa_offset 4
	.cfi_rel_offset ebx, 0
	pushl %ecx
	.cfi_adjust_cfa_offset 4
	.cfi_rel_offset ecx, 0
	pushl %edx
	.cfi_adjust_cfa_offset 4
	.cfi_rel_offset edx, 0
	pushl %ebp
	.cfi_adjust_cfa_offset 4
	.cfi_rel_offset ebp, 0
	pushl %esi
	.cfi_adjust_cfa_offset 4
	.cfi_rel_offset esi, 0
	pushl %edi
	.cfi_adjust_cfa_offset 4
	.cfi_rel_offset edi, 0

	movl 36(%esp), %ebx

	movl %dr5, %eax
	pushl %eax
	.cfi_adjust_cfa_offset 4
	movl %dr4, %eax
	pushl %eax
	.cfi_adjust_cfa_offset 4
	movl %dr3, %eax
	pushl %eax
	.cfi_adjust_cfa_offset 4
	movl %dr2, %eax
	pushl %eax
	.cfi_adjust_cfa_offset 4
	movl %dr1, %eax
	pushl %eax
	.cfi_adjust_cfa_offset 4
	movl %dr0, %eax
	pushl %eax
	.cfi_adjust_cfa_offset 4

	movl $SEL_KDATA, %eax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs

	/* Call exception handler */
	movl exceptions(,%ebx,4), %eax
	pushl %esp
	.cfi_adjust_cfa_offset 4
	pushl %ebx
	.cfi_adjust_cfa_offset 4
	call *%eax
	addl $8, %esp
	.cfi_adjust_cfa_offset -8

	popl %eax
	.cfi_adjust_cfa_offset -4
	movl %eax, %dr0
	popl %eax
	.cfi_adjust_cfa_offset -4
	movl %eax, %dr1
	popl %eax
	.cfi_adjust_cfa_offset -4
	movl %eax, %dr2
	popl %eax
	.cfi_adjust_cfa_offset -4
	movl %eax, %dr3
	popl %eax
	.cfi_adjust_cfa_offset -4
	movl %eax, %dr4
	popl %eax
	.cfi_adjust_cfa_offset -4
	movl %eax, %dr5

	popl %edi
	.cfi_adjust_cfa_offset -4
	popl %esi
	.cfi_adjust_cfa_offset -4
	popl %ebp
	.cfi_adjust_cfa_offset -4
	popl %edx
	.cfi_adjust_cfa_offset -4
	popl %ecx
	.cfi_adjust_cfa_offset -4
	popl %ebx
	.cfi_adjust_cfa_offset -4
	popl %eax
	.cfi_adjust_cfa_offset -4
	popw %gs
	.cfi_adjust_cfa_offset -2
	popw %fs
	.cfi_adjust_cfa_offset -2
	popw %es
	.cfi_adjust_cfa_offset -2
	popw %ds
	.cfi_adjust_cfa_offset -2
	addl $8, %esp
	.cfi_adjust_cfa_offset -8
	iret
	.cfi_endproc
.size _exception_handler, .-_exception_handler


/* Undefined exceptions; kept here to have shorter jumps */
.irp i, 29, 28, 27, 26, 25, 24, 23, 22, 21
EXCSTUB \i
.endr


.section .init

.globl _exception_stub_table
.align 4
.type _exception_stub_table, @object
_exception_stub_table:

.irp i, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31
	.long _exceptions_exc\i
.endr

.size _exception_stub_table, .-_exception_stub_table
