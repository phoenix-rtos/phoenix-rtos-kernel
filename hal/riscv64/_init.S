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


#define SBI_SET_TIMER 0
#define SBI_CONSOLE_PUTCHAR 1
#define SBI_CONSOLE_GETCHAR 2
#define SBI_CLEAR_IPI 3
#define SBI_SEND_IPI 4
#define SBI_REMOTE_FENCE_I 5
#define SBI_REMOTE_SFENCE_VMA 6
#define SBI_REMOTE_SFENCE_VMA_ASID 7
#define SBI_SHUTDOWN 8

#define SR_FS           0x00006000
#define SR_SUM          0x00040000
#define SR_MER          0x00080000

#include <arch/cpu.h>
#include <arch/pmap.h>


.section .init, "x"

/* a1 - contains address of dtb (reserved for use in dtb_parse() ) */
.globl _start
.type _start, @function
_start:
	/* Mask all interrupts */
	csrw sie, zero
	csrw sscratch, zero

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
	li t0, SR_FS
	csrc sstatus, t0
	/* Allow supervisor access to user space and reading from execute pages (for the time being) */
	li t0, SR_SUM | SR_MER
	csrs sstatus, t0


/* Temporary solution: code allowing skipping phoenix-rtos-loader */
	la t0, _syspage_data
	ld t0, 28(t0) /* points to syspage->progs address (circular list) */
	mv t1, t0
	beqz t1, dtb

	/* Copy application to _end + 4MB */
	la t4, _end + 4 * 1024 * 1024

	li a2, SIZE_PAGE - 1
	not a3, a2

update_apps:
	ld t2, 16(t1) /* prog->start */
	ld t3, 24(t1) /* prog->end */

	/* Align destination address to SIZE_PAGE */
	add t4, t4, a2
	and t4, t4, a3

	/* Set new start in prog->start */
	sd t4, 16(t1)

copy_app:
	ld t5, (t2)
	addi t2, t2, 8
	sd t5, (t4)
	addi t4, t4, 8
	bltu t2, t3, copy_app

	/* Set new end in prog->end */
	sd t4, 24(t1)

	ld t1, (t1)
	bne t1, t0, update_apps
/* End of the the temporary code */

dtb:
	call dtb_parse
	call _pmap_preinit

	li a1, VADDR_KERNEL
	la a0, _start
	sub a1, a1, a0

	/* Point stvec to virtual address of intruction after satp write */
	la a0, 1f
	add a0, a0, a1
	csrw stvec, a0

	/* Relocate stack */
	la sp, pmap_common
	li t0, 3 * SIZE_PAGE + SIZE_PAGE              /* pdirs + stack */
	add sp, sp, t0
	add sp, sp, a1

	/* Initialize syspage pointer */
	la t0, _syspage_data
	add t0, t0, a1
	la t1, hal_syspage
	sd t0, (t1)

	la a0, hal_relOffs
	sd a1, (a0)

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

	call main
	li a7, SBI_SHUTDOWN
	ecall

.align 4
.Lsecondary_park:
	wfi
	j .Lsecondary_park

.size _start, .-_start

.align 8
.global _syspage_data
_syspage_data:
	/* fill by syspagen */
.org _syspage_data + 0x400, 0x0
_syspage_data_end:
