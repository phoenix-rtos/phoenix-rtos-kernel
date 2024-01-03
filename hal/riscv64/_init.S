/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for RISCV64
 *
 * Copyright 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk, Łukasz Kosiński
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>
#include <arch/pmap.h>


#define VADDR_SYSPAGE (_end + SIZE_PAGE - 1)


.section .init, "x"

/* a0 - contains address of syspage
 * a1 - contains address of dtb (reserved for use in dtb_parse())
 */
.globl _start
.type _start, @function
_start:
	/* Mask all interrupts */
	csrw sie, zero
	csrw sscratch, zero
	mv tp, zero

	la sp, pmap_common + 4 * SIZE_PAGE

	/* Load the global pointer */
.option push
.option norelax
	la gp, __global_pointer$
.option pop

	/*
	 * Disable FPU to detect illegal usage of
	 * floating point in kernel space
	 */
	li t0, SSTATUS_FS
	csrc sstatus, t0
	/* Allow supervisor access to user space */
	li t0, SSTATUS_SUM
	csrs sstatus, t0

	la t0, hal_syspage /* t0 = &hal_syspage (phy) */
	li s1, VADDR_KERNEL
	la s2, _start      /* s2 = kernel start (phy) */
	la t1, VADDR_SYSPAGE
	srli t1, t1, 12
	slli t1, t1, 12
	add t2, t1, s1
	sub t2, t2, s2
	/* store virt addr of syspage in hal_syspage */
	sd t2, (t0)
	la t3, hal_relOffs
	sub t4, t2, a0 /* t4 = offset between syspage VADDR and syspage PHYADDR */
	sd t4, (t3)

	/* calculate phy addr of syspage end */
	lw t2, 4(a0) /* t2 = syspage->size */
	add t2, t2, t1

syspage_cpy:
	ld t3, (a0)
	addi a0, a0, 8
	sd t3, (t1)
	addi t1, t1, 8
	bltu t1, t2, syspage_cpy

dtb:
	call dtb_parse
	call _pmap_preinit

	/* s1 = VADDR_KERNEL, s2 = _start (phy) */
	sub a1, s1, s2

	/* Point stvec to virtual address of intruction after satp write */
	la a0, 1f
	add a0, a0, a1
	csrw stvec, a0

	/* Relocate stack */
	la sp, pmap_common
	li t0, 3 * SIZE_PAGE + SIZE_PAGE              /* pdirs + stack */
	add sp, sp, t0
	add sp, sp, a1

	la a0, pmap_common
	srl a0, a0, 12
	li a1, 0x8000000000000000
	or a0, a0, a1

	sfence.vma
	csrw sptbr, a0

.align 4
1:
	/* Add dummy page fault trap handler */
	la a0, .Lsecondary_park
	csrw stvec, a0

	j main

.align 4
.Lsecondary_park:
	wfi
	j .Lsecondary_park

.size _start, .-_start
