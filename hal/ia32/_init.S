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

#define PAGING_ENABLE 0x80000000

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

_init_setupPaging:

	/* Create empty page directory for first 4 MB mapping */
	movl $1024, %ecx
	xorl %eax, %eax

	/* Get address of page directory from SYSPAGE */
	movl %esi, %ebx
	addl $16, %ebx
	movl (%ebx), %edi
	cld
	rep stosl

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

	/* Set kernel page directory */
	movl %ecx, %cr3

	/* Enable big pages */
	movl %cr4, %eax
	orl $0x10, %eax
	movl %eax, %cr4

	/* Now enable paging */
	movl %cr0, %eax
	orl $PAGING_ENABLE, %eax
	movl %eax, %cr0

	/* Relocate stack */
	addl $VADDR_KERNEL, %esp

	pushl %esi
	lea _hal_configInit, %eax
	call *%eax
	addl $4, %esp

	/* Test LAPIC presence: it's necessary to start other cores */
	movl hal_config, %eax
	testl %eax, %eax
	jz b3

_start_initap:
	/* Get SYSPAGE address before paging */
	movl syspage, %eax
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
	cld
	rep movsb

	/* Send INI IPI */
	pushl $0x000c4500 /* 11 00 0 1 0 0 0 101 00000000 */
	pushl 0xff
	call hal_cpuSendIPI
	addl $8, %esp

	/* Send SIPI IPI (with real memory-address 0x00011000) */
	pushl $0x000c4611 /* 11 00 0 0 1 0 0 0 110 00010001 */
	pushl 0xff
	call hal_cpuSendIPI
	addl $8, %esp

b3:
	/* Now jump to main function */
	pushl $0
	pushl $0

	lea main, %eax
	pushl %eax
	ret
	hlt
.size _start, .-_start


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
	xorw %ax, %ax
	mov %ax, %ss
	movl $INIT_ESP, %esp
	movl $SYSPAGE_SEG, %eax
	shl $4, %eax
	push %eax

	/* Enable protected mode */
	mov %cr0, %eax
	orb $1, %al
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
	orl $PAGING_ENABLE, %eax
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
