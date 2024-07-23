/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for Cortex-R52 (ARMv8) architecture
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

.extern syspage

.arm

.section .init, "ax"
.org 0
_vector_table:
	b _start
	b _exception_undef
	b _syscalls_dispatch
	b _exception_prefetch
	b _exception_abort
	.word 0
	b _interrupts_dispatch
	b _interrupts_dispatch


.global _start
.type _start, %function
_start:
	/* r9 contains address of syspage from PLO */
	ldr r0, =syspage
	str r9, [r0]

	/* Disable L1 caches */
	mrc p15, 0, r1, c1, c0, 0        /* Read SCTLR (System Control Register) data  */
	bic r1, r1, #(0x1 << 12)         /* Disable ICache                             */
	bic r1, r1, #(0x1 << 2)          /* Disable DCache                             */
	mcr p15, 0, r1, c1, c0, 0        /* Write SCTLR (System Control Register) data */

	/* Invalidate L1 ICache */
	mov r1, #0
	mcr p15, 0, r1, c7, c5, 0        /* Clear ICIALLU */

	/* Invalidate L1 DCache. Based on ARM Cortex-A Series Programmer's Guide */
	mrc p15, 1, r0, c0, c0, 0        /* Read CCSIDR (Cache Size Identification Register) */
	mov r3, #0x1ff
	and r0, r3, r0, lsr #13          /* r0 = number of sets -                            */
	mov r1, #0                       /* r1 = way counter way_loop                        */
way_loop:
	mov r3, #0                       /* r3 = set counter set_loop                        */
set_loop:
	mov r2, r1, lsl #30
	orr r2, r3, lsl #5               /* r2 = set/way cache operation format              */
	mcr p15, 0, r2, c7, c6, 2        /* Invalidate line described by r2; write to DCISW  */
	add r3, r3, #1                   /* Increment set counter                            */
	cmp r0, r3                       /* Check whether last set was reached               */
	bgt set_loop                     /* Iterate set_loop                                 */
	add r1, r1, #1                   /* Increment way counter                            */
	cmp r1, #4                       /* Check whether last way was reached               */
	bne way_loop

	/* Enable L1 Caches */
	mrc p15, 0, r1, c1, c0, 0         /* Read SCTLR (System Control Register) data  */
	orr r1, r1, #(0x1 << 2)           /* Enable data cache                          */
	orr r1, r1, #(0x1 << 12)          /* Enable instruction cache                   */
	mcr p15, 0, r1, c1, c0, 0         /* Write SCTLR (System Control Register) data */
	dsb
	isb

	/* Set up VBAR */
	ldr r0, =_vector_table
	mcr p15, 0, r0, c12, c0, 0

	/* Get CPU ID */
	mrc p15, 0, r0, c0, c0, 5
	and r0, #0xf

	/* Set up stack pointer */
	add r0, #1
	ldr r1, =_init_stack

	/* Calculate stack address */
	add r0, r1, r0, lsl #INITIAL_KSTACK_SHIFT

	/* FIQ mode stack */
	msr CPSR_c, #(MODE_FIQ | NO_INT)
	mov sp, r0
	sub r0, r0, #0x20

	/* IRQ mode stack */
	msr CPSR_c, #(MODE_IRQ | NO_INT)
	mov sp, r0
	sub r0, r0, #0x100

	/* Supervisor mode stack */
	msr CPSR_c, #(MODE_SVC | NO_INT)
	mov sp, r0
	sub r0, r0, #0x40

	/* Undefined mode stack */
	msr CPSR_c, #(MODE_UND | NO_INT)
	mov sp, r0
	sub r0, r0, #0x40

	/* Abort mode stack */
	msr CPSR_c, #(MODE_ABT | NO_INT)
	mov sp, r0
	sub r0, r0, #0x40

	/* System mode stack */
	msr CPSR_c, #(MODE_SYS | NO_INT)
	mov sp, r0

	/* Enable FPU */
	mrc p15, 0, r0, c1, c0, 2                 /* Read CPACR into R0                                 */
	orr r0, r0, #((0x3 << 22) | (0x3 << 20))  /* Set CP11 and CP10: Privileged and User mode access */
	mcr p15, 0, r0, c1, c0, 2                 /* Write R0 to CPACR                                  */
	isb
	vmrs r0, fpexc
	orr r0, r0, #(0x1 << 30)                  /* FPU enable bit                                     */
	vmsr fpexc, r0
	isb

	/* Jump to main */
	ldr r0, =main
	bx r0
.size _start, .-_start
