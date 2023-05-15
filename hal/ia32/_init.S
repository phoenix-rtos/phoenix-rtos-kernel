/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for IA32 architecure (code called after kernel load)
 *
 * Copyright 2012, 2016, 2020 Phoenix Systems
 * Copyright 2001 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>
#include <arch/pmap.h>
#include "multiboot.h"

#define INIT_ESP 0x6000
#define SYSPAGE_SEG 0x200
#define INIT_CORE_BASE_ADDR 0x11000

.code32
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
	popl %ebx
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

	/* Test presence of PS/2 keyboard */
	inb $0x64, %al
	cmp $0xff, %al
	jz _init_setupPaging

	/* Disable A20 line mask */
	call _init_empty8042
	movb $0xd1, %al
	outb %al, $0x64
	call _init_empty8042
	movb $0xdf, %al
	outb %al, $0x60
	call _init_empty8042

_init_setupPaging:

	/* Create empty page directory for first 4 MB mapping */
	movl $1024, %ecx
	xorl %eax, %eax

	/* Get address of page directory from SYSPAGE */
	movl %esi, %ebx
	addl $16, %ebx
	movl (%ebx), %edi
	cld
	rep; stosl

	/* Map first 4MB of memory at VADDR_KERNEL and at 0 addresses */
	movl (%ebx), %ecx
	movl 4(%ebx), %edx
	orl $3, %edx

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

	/* Map LAPIC registers linearly into the kernel address space */
	movl %ecx, %ebx
	movl %ecx, %edi
	addl $0x1000, %edi

	std
	subl $20, %edi
	movl $0xfec0019b, %eax
	movl %eax, (%edi)
	cld
	movl %ebx, %ecx

	/* Set kernel page directory */
	movl %ecx, %cr3

	/* Enable big pages */
	movl %cr4, %eax
	orl $0x10, %eax
	movl %eax, %cr4

	/* Now enable paging */
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

	/* Test LAPIC presence */
	movl (0xfee00020), %ebx
	cmp $0xffffffff, %ebx
	jz b3

_start_initap:
	/* Get SYSPAGE address before paging */
	subl $8, %eax
	subl $VADDR_KERNEL, %eax
	movl %eax, %esi

	/* Set bootstrap flag on the top of the stack */
	movl $0xaabbccdd, %ebx
	subl $4, %esp
	movl %ebx, (%esp)

	/* Copy AP initialization code to address below 1MB */
	movl (%esi), %eax
	movl %eax, (0xfff0)
	movl 4(%esi), %eax
	movl %eax, (0xfff4)
	subl $VADDR_KERNEL, (0xfff2)
	movl %esi, (0xfff8)
	movl $INIT_CORE_BASE_ADDR, %edi
	movl $_init_core, %eax
	subl $VADDR_KERNEL, %eax
	movl %eax, %esi
	movl $0x200, %ecx
	cld;
	rep; movsb

//jmp b3

	/* Send INI IPI */
	movl $0x000c4500, %eax /* 11 00 0 1 0 0 0 101 00000000 */;
	mov %eax, (0xfee00300)
b0:
	btl $12, (0xfee00300)
	jc b0

	/* Send SIPI IPI (with real memory-address 0x00011000) */
	movl $0x000c4611, %eax /* 11 00 0 0 1 0 0 0 110 00010001 */
	mov %eax, (0xfee00300)
b1:	btl $12, (0xfee00300)
	jc b1
b3:
	/* Now jump to main function */
	pushl $0
	pushl $0

	lea main, %eax
	pushl %eax
	ret
	hlt
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


.code16
.globl _init_core
.type _init_core, @function
_init_core:
	cli
	/* Enable cache */
	mov %cr0, %eax
	andl $0x9FFFFFFF, %eax
	mov %eax, %cr0

	mov $0xfff, %ax
	mov %ax, %es
	lgdt %es:0
	mov $0, %ax
	mov %ax, %ss
	movl $INIT_ESP, %esp
	movl $SYSPAGE_SEG, %eax
	shl $4, %eax
	push %eax

	/* Enable protected mode */
	mov %cr0, %eax
	orl $1, %eax
	mov %eax, %cr0
	ljmpl $SEL_KCODE , $(INIT_CORE_BASE_ADDR + _init_core_prot - _init_core)
.code32
_init_core_prot:
	movw $SEL_KDATA, %ax
	movw %ax, %ss
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs

	/* Initialize virtual memory */
	movl (0xfff8), %esi
	movl %esi, %ebx
	addl $16, %ebx
	movl (%ebx), %ecx

	/* Set page directory */
	movl %ecx, %cr3

	/* Enable big pages */
	movl %cr4, %eax
	orl $0x10, %eax
	movl %eax, %cr4

	movl %cr0, %eax
	orl $0x80000000, %eax
	movl %eax, %cr0

	/* Switch to virtual addresses */
	lea _init_virt, %eax
	jmp *%eax

_init_virt:
	/* Reload relocated GDT and IDT */
	addl $VADDR_KERNEL, %esi
	lgdt (%esi)
	addl $8, %esi
	lidt (%esi)

	/* Get BSP stack and establish bootstrap flag pointer */
	addl $16, %esi
	movl (%esi), %ebx
	addl $VADDR_KERNEL, %ebx
	subl $4, %ebx

_init_core_wait:
	movl $0xaa, %eax
	xchgl (%ebx), %eax
	cmp $0, %eax
	jne _init_core_wait

	movl $0x7c00, %esp
	addl $VADDR_KERNEL, %esp
	lea _cpu_initCore, %eax
	call *%eax
	movl %eax, %esp

	/* Signal spinlock */
	xorl %eax, %eax
	xchg (%ebx), %eax
	sti
_init_core_stop:
	hlt
	jmp _init_core_stop

.size _init_core, .-_init_core


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
