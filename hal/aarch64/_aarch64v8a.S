/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level hal functions for AArch64
 *
 * Copyright 2018, 2020, 2021-2022 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Maciej Purski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

.arch armv8-a

.globl hal_cpuGetCycles
.type hal_cpuGetCycles, %function
hal_cpuGetCycles:
	mrs x1, PMCCNTR_EL0
	str x1, [x0]
	ret
.size hal_cpuGetCycles, .-hal_cpuGetCycles
.ltorg


.globl _hal_cpuSetKernelStack
.type _hal_cpuSetKernelStack, %function
_hal_cpuSetKernelStack:
	dsb ish
	msr tpidr_el1, x0
	dsb	ish
	isb
	ret
.size _hal_cpuSetKernelStack, .-_hal_cpuSetKernelStack
.ltorg


.globl hal_cpuGetProcID
.type hal_cpuGetProcID, %function
hal_cpuGetProcID:
	mrs x1, id_aa64mmfr0_el1
	mrs x2, id_aa64pfr0_el1
	stp x1, x2, [x0], #16
	mrs x1, id_aa64isar0_el1
	str x1, [x0], #8
	mrs x1, id_aa64dfr0_el1
	mrs x2, midr_el1
	stp w1, w2, [x0], #8
	ret
.size hal_cpuGetProcID, .-hal_cpuGetProcID
.ltorg


.globl hal_jmp /* void hal_jmp(void *f, void *kstack, void *ustack, int kargc, const arg_t *kargs) */
.type hal_jmp, %function
hal_jmp:
	msr daifSet, #3
	mov x19, x0
	mov x20, x1
	mov x21, x2
	/* ustack != NULL means that the jump is to user */
	cbnz x21, 2f
	mov sp, x20
	/* pass args */
	subs x3, x3, #1
	b.mi 1f
	ldr x0, [x4]
	subs x3, x3, #1
	b.mi 1f
	ldr x1, [x4, #8]
	subs x3, x3, #1
	b.mi 1f
	ldr x2, [x4, #16]
	subs x3, x3, #1
	b.mi 1f
	ldr x3, [x4, #24]
1:	msr daifClr, #3
	blr x19
	/* TODO: what if the function above returns? it seems that other implementations of hal_jmp also don't handle this */
	udf #0 /* Cause an exception if the function returns */

	/* Handle userspace jump */
2:	mov sp, x21
	msr elr_el1, x19
	mov x20, #MODE_EL0
	msr spsr_el1, x20
	eret
.size hal_jmp, .-hal_jmp
.ltorg
