/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level hal functions for ARMv8-R
 *
 * Copyright 2018, 2020-2022, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Maciej Purski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

.arm

.globl hal_cpuGetCycles
.type hal_cpuGetCycles, %function
hal_cpuGetCycles:
	mrc p15, 0, r1, c9, c13, 0 /* PMCCNTR */
	str r1, [r0]
	bx lr
.size hal_cpuGetCycles, .-hal_cpuGetCycles
.ltorg


.globl hal_cpuBranchInval
.type hal_cpuBranchInval, %function
hal_cpuBranchInval:
	dsb
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 6 /* BPIALL */
	dsb
	isb
	bx lr
.size hal_cpuBranchInval, .-hal_cpuBranchInval
.ltorg


.globl hal_cpuICacheInval
.type hal_cpuICacheInval, %function
hal_cpuICacheInval:
	dsb
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0 /* ICIALLU */
	dsb
	isb
	bx lr
.size hal_cpuICacheInval, .-hal_cpuICacheInval
.ltorg


.globl hal_cpuInvalDataCache
.type hal_cpuInvalDataCache, %function
hal_cpuInvalDataCache:
	dsb
	mrc p15, 0, r3, c0, c0, 1 /* Read the Cache Type Register */
	lsr r3, r3, #16           /* DMinLine value */
	and r3, r3, #0xf
	mov r2, #4
	mov r2, r2, lsl r3        /* Cache line size in bytes */
	sub r3, r2, #1            /* Cache line size mask */
	bic r0, r0, r3
inval_line:
	mcr p15, 0, r0, c7, c6, 1 /* DCIMVAC */
	add r0, r0, r2
	cmp r0, r1
	blo inval_line
	dsb
	isb
	bx lr
.size hal_cpuInvalDataCache, .-hal_cpuInvalDataCache
.ltorg


.globl hal_cpuCleanDataCache
.type hal_cpuCleanDataCache, %function
hal_cpuCleanDataCache:
	dsb
	mrc p15, 0, r3, c0, c0, 1  /* Read the Cache Type Register */
	lsr r3, r3, #16            /* DMinLine value */
	and r3, r3, #0xf
	mov r2, #4
	mov r2, r2, lsl r3         /* Cache line size in bytes */
	sub r3, r2, #1             /* Cache line size mask */
	bic r0, r0, r3
clean_line:
	mcr p15, 0, r0, c7, c10, 1 /* DCCMVAC */
	add r0, r0, r2
	cmp r0, r1
	blo clean_line
	dsb
	isb
	bx lr
.size hal_cpuCleanDataCache, .-hal_cpuCleanDataCache
.ltorg


.globl hal_cpuFlushDataCache
.type hal_cpuFlushDataCache, %function
hal_cpuFlushDataCache:
	dsb
	mrc p15, 0, r3, c0, c0, 1  /* Read the Cache Type Register */
	lsr r3, r3, #16            /* DMinLine value */
	and r3, r3, #0xf
	mov r2, #4
	mov r2, r2, lsl r3         /* Cache line size in bytes */
	sub r3, r2, #1             /* Cache line size mask */
	bic r0, r0, r3
flush_line:
	mcr p15, 0, r0, c7, c14, 1 /* DCCIMVAC */
	add r0, r0, r2
	cmp r0, r1
	blo flush_line
	dsb
	isb
	bx lr
.size hal_cpuFlushDataCache, .-hal_cpuFlushDataCache
.ltorg


.globl hal_cpuGetMIDR
.type hal_cpuGetMIDR, %function
hal_cpuGetMIDR:
	mrc p15, 0, r0, c0, c0, 0
	bx lr
.size hal_cpuGetMIDR, .-hal_cpuGetMIDR
.ltorg


.globl hal_cpuGetPFR0
.type hal_cpuGetPFR0, %function
hal_cpuGetPFR0:
	mrc p15, 0, r0, c0, c1, 0
	bx lr
.size hal_cpuGetPFR0, .-hal_cpuGetPFR0
.ltorg


.globl hal_cpuGetPFR1
.type hal_cpuGetPFR1, %function
hal_cpuGetPFR1:
	mrc p15, 0, r0, c0, c1, 1
	bx lr
.size hal_cpuGetPFR1, .-hal_cpuGetPFR1
.ltorg


.globl _hal_cpuSetKernelStack
.type _hal_cpuSetKernelStack, %function
_hal_cpuSetKernelStack:
	dsb
	mcr p15, 0, r0, c13, c0, 4 /* TPIDRPRW */
	dsb
	isb
	bx lr
.size _hal_cpuSetKernelStack, .-_hal_cpuSetKernelStack
.ltorg


.globl hal_jmp /* void hal_jmp(void *f, void *kstack, void *ustack, int kargc, const arg_t *kargs) */
.type hal_jmp, %function
hal_jmp:
	cpsid if
	/* ustack != NULL means that the jump is to user */
	cmp r2, #0
	bne 2f
	/* kargs are passed on the stack */
	ldr r5, [sp]
	mov r4, r0
	mov sp, r1
	/* pass args */
	subs r3, #1
	bmi 1f
	ldr r0, [r5]
	subs r3, #1
	bmi 1f
	ldr r1, [r5, #4]
	subs r3, #1
	bmi 1f
	ldr r2, [r5, #8]
	subs r3, #1
	bmi 1f
	ldr r3, [r5, #12]
1:	cpsie if
	blx r4
	/* Handle userspace jump */
2:	mov sp, r2
	cps #MODE_IRQ
	mov r1, #0x10
	tst r0, #1
	orrne r1, r1, #THUMB_STATE
	push {r0, r1}
	rfefd sp!
.size hal_jmp, .-hal_jmp
.ltorg
