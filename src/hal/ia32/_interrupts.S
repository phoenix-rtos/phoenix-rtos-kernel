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


#ifndef NDEBUG
#define CTXPUSHL 14
#else
#define CTXPUSHL 10
#endif


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
	.cfi_startproc
	.cfi_signal_frame
	// FIXME: deduplicate this:
	.cfi_escape /* esp = CFA + 8; if (*CFA & 3 != 0) esp = *esp */ \
		0x16, 4, 13,    /* DW_CFA_val_expression esp, block: */ \
		0x12,                   /* DW_OP_dup                    | cfa, cfa */ \
		0x23, 8,                /* DW_OP_plus_uconst 8          | esp=cfa+8, cfa */ \
		0x16,                   /* DW_OP_swap                   | cfa, esp */ \
		0x06,                   /* DW_OP_deref                  | cs, esp */ \
		0x33,                   /* DW_OP_lit3                   | 3, cs, esp */ \
		0x1a,                   /* DW_OP_and                    | cs & 3, esp */ \
		0x30,                   /* DW_OP_lit0                   | 0, cs & 3, esp */ \
		0x29,                   /* DW_OP_eq                     | cs & 3 == 0, esp */ \
		0x28, 1, 0,             /* DW_OP_bra +1                 | esp */ \
		0x06                    /* DW_OP_deref                  | *esp */

	call interrupts_pushContext
	.cfi_adjust_cfa_offset CTXPUSHL * 4
	.cfi_rel_offset eax, (CTXPUSHL - 3) * 4
	.cfi_rel_offset ebx, (CTXPUSHL - 4) * 4
	.cfi_rel_offset ecx, (CTXPUSHL - 5) * 4
	.cfi_rel_offset edx, (CTXPUSHL - 6) * 4
	.cfi_rel_offset ebp, (CTXPUSHL - 7) * 4
	.cfi_rel_offset esi, (CTXPUSHL - 8) * 4
	.cfi_rel_offset edi, (CTXPUSHL - 9) * 4
	movl $SEL_KDATA, %edx
	movw %dx, %ds
	movw %dx, %es
	movl (4 * CTXPUSHL + 12)(%esp), %edx
	pushl %edx
	.cfi_adjust_cfa_offset 4
	pushl %eax
	.cfi_adjust_cfa_offset 4
	call syscalls_dispatch
	addl $8, %esp
	.cfi_adjust_cfa_offset -8
	movl %eax, (4 * CTXPUSHL - 12)(%esp)
	jmp interrupts_popContext
	.cfi_endproc
.size _interrupts_syscall, .-_interrupts_syscall
