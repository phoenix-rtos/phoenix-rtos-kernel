/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt stubs
 *
 * Copyright 2012, 2016, 2020 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author; Pawel Pisarczyk, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

.extern _interrupts_multilock;

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


.global _interrupts_multilockSet
_interrupts_multilockSet:
	pushl %eax
	xorl %eax, %eax
_interrupts_multilockSet_l1:
	xchgl _interrupts_multilock, %eax
	orl %eax, %eax
	jz _interrupts_multilockSet_l1
	popl %eax
	ret
.size _interrupts_multiLockSet, .-_interrupts_multilockSet


.global _interrupts_multilockClear
_interrupts_multilockClear:
	pushl %eax
	xorl %eax, %eax
	incl %eax
	xchgl _interrupts_multilock, %eax
	popl %eax
	ret
.size _interrupts_multilockClear, .-_interrupts_multilockClear


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
	movl %eax, %ebx; \
	addl $8, %esp; \
	push $intr; \
	call _interrupts_apicACK; \
	addl $4, %esp; \
	cmpl $0, %ebx; \
	jz interrupts_popContext; \
	leal 0(%esp), %eax; \
	pushl $0; \
	pushl %eax; \
	pushl $0; \
	call _interrupts_multilockSet; \
	call threads_schedule; \
	addl $12, %esp; \
	popl %esp; \
	call _interrupts_multilockClear; \
	pushl %esp; \
	jmp interrupts_popContext; \
.size name, .-name


/* Interrupt routine for scheduler */
.globl _interrupts_irq0
.type _interrupts_irq0, @function
.align 4, 0x90
_interrupts_irq0:
	call interrupts_pushContext
	movl $SEL_KDATA, %eax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs

	call _interrupts_multilockSet

	/* Dispatch IRQ */
	pushl %esp
	pushl $0
	call interrupts_dispatchIRQ
	movl %eax, %ebx
	addl $8, %esp

	pushl $0
	call _interrupts_apicACK
	addl $4, %esp

	/* Unlock scheduler */
	popl %esp
	call _interrupts_multilockClear
	pushl %esp

	jmp interrupts_popContext
.size _interrupts_irq0, .-_interrupts_irq0


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
	sti
	call syscalls_dispatch
	cli
	addl $8, %esp
	movl %eax, (4 * CTXPUSHL - 12)(%esp)
	jmp interrupts_popContext
.size _interrupts_syscall, .-_interrupts_syscall
