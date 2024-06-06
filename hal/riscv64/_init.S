/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for RISCV64
 *
 * Copyright 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Łukasz Kosiński, Lukasz Leczkowski
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

/* a0 - contains hart ID
 * a1 - contains address of syspage
 * a2 - contains address of dtb (reserved for use in dtb_parse())
 */
.globl _start
.type _start, @function
_start:
	/* Mask all interrupts */
	csrw sie, zero

	/* Save per-hart data pointer */
	la t0, hal_riscvHartData
	/* offset = hartId * 24 (sizeof(hal_riscvHartData_t)) */
	slli t1, a0, 1
	add t1, t1, a0
	slli t1, t1, 3
	add t0, t0, t1
	/* save hartId */
	sd a0, (t0)
	la t1, _start
	sub t0, t0, t1
	li t2, VADDR_KERNEL
	add t0, t0, t2
	csrw sscratch, t0
	mv s0, a0

	/*
	 * Disable FPU to detect illegal usage of
	 * floating point in kernel space
	 */
	li t0, SSTATUS_FS
	csrc sstatus, t0
	/* Allow supervisor access to user space */
	li t0, SSTATUS_SUM
	csrs sstatus, t0

	/* Load the global pointer */
.option push
.option norelax
	la gp, __global_pointer$
.option pop

	li s1, VADDR_KERNEL
	la s2, _start  /* s2 = kernel start (phy) */

	/* Check if current hart is the boot hart */
	lw s3, (a1)  /* s3 = syspage->hs.boothartId */
	bne s0, s3, _init_core

	la sp, pmap_common
	li t0, 3 * SIZE_PAGE + SIZE_INITIAL_KSTACK
	add sp, sp, t0
	slli t0, s0, INITIAL_KSTACK_BIT
	add sp, sp, t0

	la t0, hal_syspage /* t0 = &hal_syspage (phy) */
	la t1, VADDR_SYSPAGE
	srli t1, t1, 12
	slli t1, t1, 12
	add t2, t1, s1
	sub t2, t2, s2
	/* store virt addr of syspage in hal_syspage */
	sd t2, (t0)
	la t3, hal_relOffs
	sub t4, t2, a1 /* t4 = offset between syspage VADDR and syspage PHYADDR */
	sd t4, (t3)

	/* calculate phy addr of syspage end */
	lw t2, 4(a1)  /* t2 = syspage->size */
	add t2, t2, t1

syspage_cpy:
	ld t3, (a1)
	addi a1, a1, 8
	sd t3, (t1)
	addi t1, t1, 8
	bltu t1, t2, syspage_cpy

dtb:
	mv s4, a2
	mv a0, a2
	call dtb_save
	mv a0, s4
	call _pmap_preinit

_init_core:

	/* s1 = VADDR_KERNEL, s2 = _start (phy) */
	sub a1, s1, s2

	/* Point stvec to virtual address of intruction after satp write */
	la a0, 1f
	add a0, a0, a1
	csrw stvec, a0

	/* Relocate stack */
	la sp, pmap_common
	li t0, 3 * SIZE_PAGE + SIZE_INITIAL_KSTACK
	add sp, sp, t0
	slli t0, s0, INITIAL_KSTACK_BIT
	add sp, sp, t0
	add sp, sp, a1

	la a0, pmap_common
	srl a0, a0, 12
	li a1, SATP_MODE_SV39
	or a0, a0, a1

	sfence.vma
	csrw satp, a0

.align 4
1:

.option push
.option norelax
	la gp, __global_pointer$
.option pop

	/* Add dummy page fault trap handler */
	la a0, .Lsecondary_park
	csrw stvec, a0

	/* s0 = hartId, s3 = bootHartId
	 * Boot hart will continue to main
	 */
	beq s0, s3, main

	call hal_cpuInitCore

	/* Enable IPI irq */
	csrs sie, (1 << 1)
	csrs sstatus, 2
2:
	call hal_started
	fence rw, rw
	beqz a0, 2b

	/* Enable interrupts */
	li a0, -1
	csrw sie, a0


.align 4
.Lsecondary_park:
	wfi
	j .Lsecondary_park

.size _start, .-_start
