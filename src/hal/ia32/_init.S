/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for IA32 architecure (code called after kernel load)
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "cpu.h"
#include "pmap.h"
#include "multiboot.h"


.section .init, "x"

/* .init */
.globl _start
.align 4, 0x90
.type _start, @function
_start:
	cmpl $MULTIBOOT_INFO_MAGIC, %eax
	jne _init_plo

	/* setup initial kernel stack (one page) */
	movl $(multiboot_common + SIZE_PAGE), %esp
	subl $VADDR_KERNEL, %esp

	/* convert multiboot info to syspage etc. */
	push %ebx
	call _multiboot_init
	movl %eax, %esi
	jmp _init_setupPaging

_init_plo:
	movw $SEL_KDATA, %ax
	movw %ax, %ss
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs

	/* Locate system page */
	movl %esp, %eax

	/* Store system page address in esi register */
	movl (%eax), %esi

	/* Disable A20 line mask */
	call _init_empty8042
	movb $0xd1, %al
	outb %al, $0x64
	call _init_empty8042
	movb $0xdf, %al
	outb %al, $0x60
	call _init_empty8042

	/* Create empty page directory */
_init_setupPaging:
	movl $1024, %ecx
	xorl %eax, %eax
	movl %esi, %ebx
	addl $16, %ebx
	movl (%ebx), %edi
	cld
	rep; stosl

	/* Map first 4MB of memory at VADDR_KERNEL and at 0 adresses */
	movl (%ebx), %ecx
	movl 4(%ebx), %edx
	orl $3, %edx

/*	movl $(VADDR_KERNEL >> 22), %eax
	shl $2, %eax*/

	movl $(VADDR_KERNEL >> 20), %eax
	addl %ecx, %eax
	movl %edx, (%ecx)
	movl %edx, (%eax)
	andl $0xfffff000, %edx

	/* Create first page table for kernel */
	movl %edx, %edi
	addl $0x1000, %edi
	subl $4, %edi

	movl $0x03ff003, %eax
	std
1:
	stosl
	subl $0x1000, %eax
	jge 1b
	cld

	/* Now enable paging */
	movl %ecx, %cr3
	movl %cr0, %eax
	orl $0x80000000, %eax
	movl %eax, %cr0

	/* Store pointer to syspage in kernel variable */
	addl $VADDR_KERNEL, %esi
	movl %esi, syspage

	/* Relocate stack, GDT and IDT */
	addl $VADDR_KERNEL, %esp
	addl $2, %esi
	addl $VADDR_KERNEL, (%esi)
	addl $8, %esi
	addl $VADDR_KERNEL, (%esi)

	movl syspage, %eax
	lgdt (%eax)
	addl $8, %eax
	lidt (%eax)

	/* Now jump to main function */
	pushl $0
	pushl $0
	lea main, %eax
	pushl %eax
	ret
.size _start, .-_start


/* Function flushes keyboard controler buffer */
.globl _init_empty8042
.align 4, 0x90
.type _init_empty8042, @function
_init_empty8042:
	pushf
	pushl %eax
	pushl %ecx
2:
	/* Short delay */
	mov $0x1000, %ecx
3:
	rep; loop 3b

	/* Test if output buffer is empty */
	inb $0x64, %al
	test $1, %al
	jz 5f

	/* Second delay */
	mov $0x1000, %ecx
4:
	rep; loop 4b

	/* Read character */
	in $0x60, %al
	jmp 2b
5:
	/* Test input keyboard buffer - if it isn't empy repeat all steps */
	test $2, %al
	jnz 2b
	pop %eax
	pop %ecx
	popf
	ret
.size _init_empty8042, .-_init_empty8042


/* Multiboot header - used when loading by multiboot compliant loader */
.align 4
.type _multiboot_header, @object
_multiboot_header:
	.long MULTIBOOT_HDR_MAGIC
	.long MULTIBOOT_HDR_PAGEALIGN | MULTIBOOT_HDR_MEMINFO | MULTIBOOT_HDR_AOUT
	.long -(MULTIBOOT_HDR_MAGIC + MULTIBOOT_HDR_PAGEALIGN + MULTIBOOT_HDR_MEMINFO + MULTIBOOT_HDR_AOUT)
	.long _multiboot_header - VADDR_KERNEL
	.long .init - VADDR_KERNEL
	.long .bss - VADDR_KERNEL
	.long end - VADDR_KERNEL
	.long _start - VADDR_KERNEL
.size _multiboot_header, .-_multiboot_header
