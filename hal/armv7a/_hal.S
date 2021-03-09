/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level hal functions for Cortex-A9
 *
 * Copyright 2018, 2020, 2021 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Maciej Purski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "cpu.h"

.arm

.globl hal_cpuGetCycles
.type hal_cpuGetCycles, %function
hal_cpuGetCycles:
	mrc p15, 0, r1, c9, c13, 0
	str r1, [r0]
	bx lr
.size hal_cpuGetCycles, .-hal_cpuGetCycles


.globl hal_cpuInvalDataCache
.type hal_cpuInvalDataCache, %function
hal_cpuInvalDataCache:
	ldr r1, =SIZE_CACHE_LINE - 1
	bic r0, r0, r1
	mcr p15, 0, r0, c7, c6, 1
	bx lr
.size hal_cpuInvalDataCache, .-hal_cpuInvalDataCache


.globl hal_cpuFlushDataCache
.type hal_cpuFlushDataCache, %function
hal_cpuFlushDataCache:
	ldr r1, =SIZE_CACHE_LINE - 1
	bic r0, r0, r1
	mcr p15, 0, r0, c7, c14, 1
	bx lr
.size hal_cpuFlushDataCache, .-hal_cpuFlushDataCache


.globl hal_cpuCleanDataCache
.type hal_cpuCleanDataCache, %function
hal_cpuCleanDataCache:
	ldr r1, =SIZE_CACHE_LINE - 1
	bic r0, r0, r1
	mcr p15, 0, r0, c7, c11, 1
	bx lr
.size hal_cpuCleanDataCache, .-hal_cpuCleanDataCache


.globl hal_cpuInvalASID
.type hal_cpuInvalASID, %function
hal_cpuInvalASID:
	and r0, r0, #0xff
	mcr p15, 0, r0, c8, c7, 2
	bx lr
.size hal_cpuInvalASID, .-hal_cpuInvalASID


.globl hal_cpuInvalTLB
.type hal_cpuInvalTLB, %function
hal_cpuInvalTLB:
	mcr p15, 0, r0, c8, c7, 0
	bx lr
.size hal_cpuInvalTLB, .-hal_cpuInvalTLB


.globl hal_cpuInvalVA
.type hal_cpuInvalVA, %function
hal_cpuInvalVA:
	mcr p15, 0, r0, c8, c7, 1 /* ASID match */
	bx lr
.size hal_cpuInvalVA, .-hal_cpuInvalVA


.globl hal_cpuBranchInval
.type hal_cpuBranchInval, %function
hal_cpuBranchInval:
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 6
	bx lr
.size hal_cpuBranchInval, .-hal_cpuBranchInval


.globl hal_cpuICacheInval
.type hal_cpuICacheInval, %function
hal_cpuICacheInval:
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0
	bx lr
.size hal_cpuICacheInval, .-hal_cpuICacheInval


.globl hal_cpuGetUserTT
.type hal_cpuGetUserTT, %function
hal_cpuGetUserTT:
	mrc p15, 0, r0, c2, c0, 0
	bx lr
.size hal_cpuGetUserTT, .-hal_cpuGetUserTT


.globl hal_cpuSetUserTT
.type hal_cpuSetUserTT, %function
hal_cpuSetUserTT:
	mcr p15, 0, r0, c2, c0, 0
	bx lr
.size hal_cpuSetUserTT, .-hal_cpuSetUserTT


.globl hal_cpuSetContextId
.type hal_cpuSetContextId, %function
hal_cpuSetContextId:
	mcr p15, 0, r0, c13, c0, 1
	bx lr
.size hal_cpuSetContextId, .-hal_cpuSetContextId


.globl hal_cpuGetContextId
.type hal_cpuGetContextId, %function
hal_cpuGetContextId:
	mrc p15, 0, r0, c13, c0, 1
	bx lr
.size hal_cpuGetContextId, .-hal_cpuGetContextId


.globl _hal_cpuSetKernelStack
.type _hal_cpuSetKernelStack, %function
_hal_cpuSetKernelStack:
	mcr p15, 0, r0, c13, c0, 4
	dsb
	isb
	bx lr
.size _hal_cpuSetKernelStack, .-_hal_cpuSetKernelStack


.globl hal_cpuGetMIDR
.type hal_cpuGetMIDR, %function
hal_cpuGetMIDR:
	mrc p15, 0, r0, c0, c0, 0
	bx lr
.size hal_cpuGetMIDR, .-hal_cpuGetMIDR


.globl hal_cpuGetPFR0
.type hal_cpuGetPFR0, %function
hal_cpuGetPFR0:
	mrc p15, 0, r0, c0, c1, 0
	bx lr
.size hal_cpuGetPFR0, .-hal_cpuGetPFR0


.globl hal_cpuGetPFR1
.type hal_cpuGetPFR1, %function
hal_cpuGetPFR1:
	mrc p15, 0, r0, c0, c1, 1
	bx lr
.size hal_cpuGetPFR1, .-hal_cpuGetPFR1


.globl hal_longjmp
.type hal_longjmp, %function
hal_longjmp:
	cpsid if
	add sp, r0, #8
	b _hal_cpuRestoreCtx
.size hal_longjmp, .-hal_longjmp


.globl hal_jmp
.type hal_jmp, %function
hal_jmp:
	cpsid if
	mov r4, r0
	mov r5, r1
	mov r6, r2
	mov r7, r3
	cmp r6, #0
	bne 2f
	mov sp, r5
	subs r7, #1
	bmi 1f
	pop {r0}
	subs r7, #1
	bmi 1f
	pop {r1}
	subs r7, #1
	bmi 1f
	pop {r2}
	subs r7, #1
	bmi 1f
	pop {r3}
1:	cpsie if
	blx r4
2:	mov sp, r6
	subs r7, #1
	bmi 3f
	pop {r0}
	subs r7, #1
	bmi 3f
	pop {r1}
	subs r7, #1
	bmi 3f
	pop {r2}
	subs r7, #1
	bmi 3f
	pop {r3}
3:	cps #0x12
	mov r5, #0x10
	tst r4, #1
	orrne r5, r5, #(1 << 5)
	push {r5}
	push {r4}
	rfefd sp!
.size hal_jmp, .-hal_jmp
