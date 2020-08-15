/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for RISCV64
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
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

#include "cpu.h"
#include "pmap.h"


.section .init, "x"

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

	/* Initialize syspage */
	la a0, syspage
	la t0, pmap_common
	li t1,  3 * SIZE_PAGE + SIZE_PAGE + SIZE_PAGE /* pdirs + stack + heap */
	add t0, t0, t1
	sd t0, (a0)

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

	/* Relocate syspage */
	la a0, syspage
	ld t0, (a0)
	add t0, t0, a1
	sd t0, (a0)

	la a0, pmap_common
	srl a0, a0, 12
	li a1, 0x8000000000000000
	or a0, a0, a1

	sfence.vma
	csrw sptbr, a0
1:
	/* Add dummy page fault trap handler */
	la a0, .Lsecondary_park
	csrw stvec, a0

	call main
	li a7, SBI_SHUTDOWN
	ecall

.Lsecondary_park:
	wfi
	j .Lsecondary_park



.size _start, .-_start
