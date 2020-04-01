/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for Cortex-M7 (ARMv7) architecture
 *
 * Copyright 2012, 2016-2017, 2020 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "cpu.h"

.syntax unified
.cpu cortex-m7


.extern _cpu_nctx
.extern syscalls
.extern syspage

#define IOMUXC_GPR 0x400ac000
#define FLEXSPI2_BASE 0x70000000

#define ADDR_ITCM 0x00000000
#define ADDR_DTCM 0x20000000
#define ADDR_SCB  0xe000ed00
#define SWAP(x)   (((x >> 24) & 0xff) | ((x << 8) & (0xff << 16)) | ((x >> 8) & (0xff << 8)) | ((x << 24) & (0xff << 24)))


.section .init, "x"

_fcfb:
.word 0x42464346 /* Tag */
.word 0x56010400 /* Version */
.word 0
.byte 0x1        /* readSampleClkSrc */
.byte 0x3        /* dataHoldTime */
.byte 0x3        /* dataSetupTime */
.byte 0          /* columnAdressWidth */
.byte 0          /* deviceModeCfgEnable */
.byte 0
.byte 0, 0       /* waitTimeCfgCommnads */
.word 0          /* deviceModeSeq */
.word 0          /* deviceModeArg */
.byte 0          /* configCmdEnable */
.byte 0, 0, 0
.word 0, 0, 0, 0 /* configCmdSeqs */
.word 0, 0, 0, 0 /* cfgCmdArgs */
.word 0x00000000 /* controllerMiscOption */
.byte 0x0          /* deviceType */
.byte 0x4        /* sflashPadType */
.byte 0x6        /* serialClkFreq */
.byte 0x0        /* lutCustomSeqEnable */
.word 0, 0
.word 0x800000 /* sflashA1Size */
.word 0          /* sflashA2Size */
.word 0          /* sflashB1Size */
.word 0          /* sflashB2Size */
.word 0          /* csPadSettingOverride */
.word 0          /* sclkPadSettingOverride */
.word 0          /* dataPadSettingOverride */
.word 0          /* dqsPadSettingOverride */
.word 0          /* timeoutInMs */
.word 0          /* commandInterval */
.word 0x00000000 /* dataValidTime */
.byte 0x0, 0     /* busyOffset */
.byte 0x0, 0     /* busyBitPolarity */

/* lookupTable */
.word 0x0a1804eb, 0x26043206, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000

/* lutCustomSeq */
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000
.word 0x00000000, 0x00000000, 0x00000000, 0x00000000

.word 0x00000100 /* pageSize */
.word 0x00001000 /* sectorSize */
.word 0x00000000 /* ipCmdSerialClkFreq */
.word 0x00000000
.word 0x00040000

.org _fcfb + 512

.type _syspage, %object
_syspage:

.org _fcfb + 0x1000, 0x0

ivt_flash:
	.word 0x412000d1                                /* hdr */
	.word _start + FLEXSPI2_BASE             /* entry */
	.word 0                                         /* reserved 1 */
	.word 0                                         /* dcd */
	.word flash_boot_data + FLEXSPI2_BASE    /* boot_data */
	.word ivt_flash + FLEXSPI2_BASE          /* self */
	.word 0                                         /* csf */
	.word 0                                         /* reserved 2 */

flash_boot_data:
	.word FLEXSPI2_BASE                      /* load address */
	.word __bss_start__ - _fcfb                     /* size */
	.word 0                                         /* plugin */
	.word 0xffffffff                                /* empty - extra data word */

ivt_flash_end:

.org _fcfb + 0x1050, 0x0

ivt:
	.word 0x412000d1                    /* hdr */
	.word _start                        /* entry */
	.word 0                             /* reserved 1 */
	.word 0                             /* dcd */
	.word boot_data                     /* boot_data */
	.word ivt                           /* self */
	.word 0                             /* csf */
	.word 0                             /* reserved 2 */

boot_data:
	.word ADDR_ITCM                     /* load address */
	.word __bss_start__ - _fcfb         /* size */
	.word 0                             /* plugin */
	.word 0xffffffff

ivt_end:

.org _fcfb + 0x1100, 0x0

.globl _init_vectors
.type _init_vectors, %object
_init_vectors:
.word _end + 1024 + 256
.word _start

.word _exceptions_dispatch /* NMI */
.word _exceptions_dispatch /* HardFault */
.word _exceptions_dispatch /* MemMgtFault */
.word _exceptions_dispatch /* BusFault */
.word _exceptions_dispatch /* UsageFault */
.word 0
.word 0
.word 0
.word 0
.word _syscall_dispatch    /* SVC */
.word _exceptions_dispatch /* Debug */
.word 0
.word _pendsv              /* PendSV */
.word _interrupts_dispatch /* Systick */

.rept 217
.word _interrupts_dispatch
.endr
.size _init_vectors, .-_init_vectors

.thumb
.thumb_func

.globl _start
.type _start, %function
_start:
	cpsid if

	/* Enable FPU */
	ldr r0, =0xe000ed88
	ldr r1, [r0]
	orr r1, r1, #(0xf << 20)
	str r1, [r0]

	/* Enable FPU ctx */
	mrs r0, control
	orr r0, r0, #(1 << 2)
	msr control, r0

	/* Check kernel localization */
	ldr r0, =FLEXSPI2_BASE
	mov r1, pc
	cmp r0, r1
	bgt _start_1

	/* Prepare registers to kernel copy */
	ldr r5, =__bss_start__
	ldr r6, =_fcfb
	sub r1, r5, r6
	ldr r2, =FLEXSPI2_BASE
	add r1, r1, r2
	ldr r3, =ADDR_ITCM

	/* Configure FLEX RAM */
	ldr r8, =IOMUXC_GPR
	ldr r9, =0x00200007
	str r9, [r8, #0x40]

	ldr r9, =0xaaaaffff
	str r9, [r8, #0x44]


 _start_copy:
	/* Copy kernel from internal flash to ITCM */
	ldr r0, [r2], #4
	str r0, [r3], #4

	cmp r2, r1
	blt _start_copy

	/* Find syspage size */
	ldr r9, =_syspage
	ldr r9, [r9, #12] //syspage size

	/* If syspage is empty, programs are not copied */
	cmp r9, #0
	beq _start_1

	/* Calcuclate offset to of the last program */
	sub r9, r9, #1

	mov r8, #0x18
	mul r9, r9, r8
	add r9, r9, #20

	ldr r8, =_syspage
	add r8, r8, r9

	ldr r8, [r8]
	add r8, r8, FLEXSPI2_BASE


 _start_syspage_copy:
	 /* Copy syspage's programs form internal flash to ITCM */
	 ldr r0, [r2], #4
	 str r0, [r3], #4

	 cmp r2, r8
	 blt _start_syspage_copy

_start_1:
	/* Init vector table and stack pointer */
	ldr r0, =0xe000ed08
	ldr r1, =_init_vectors
	str r1, [r0]
	ldr r0, [r1]
	msr msp, r0
	bl _imxrt_init

	ldr r0, =syspage
	ldr r1, =_syspage

	str r1, [r0]

	ldr r8, =main
	bx r8
.size _start, .-_start
.ltorg


.globl _syscall_dispatch
.type _syscall_dispatch, %function
.type _syscallend, %function

_syscall_dispatch:
	cpsid if
	stmdb sp!, {r4-r8}
	mrs r0, psp

	ldmia r0!, {r1-r8}
	orr r7, r7, #1
	stmdb sp!, {r1-r8}

	add r6, r0, #(18 * 4)
	stmdb sp!, {r6}
	sub sp, sp, #(18 * 4)
	mov r5, #(17 * 4)

_syscall_dispatch0:
	ldr r6, [r0, r5]
	str r6, [sp, r5]
	subs r5, r5, #4
	it pl
	bpl _syscall_dispatch0

	ldr r6, =0xe000ef38
	str sp, [r6]

	add r0, r0, #(18 * 4)
	stmdb r0!, {r1-r4}

	mov r1, r0
	ldrb r0, [r7, #-3]
	ldr r6, =syscalls_dispatch

	mov r7, #0x01000000
	ldr r5, =_syscallend
	stmdb sp!, {r0-r7}

	mov lr, #(1 << 2)
	msr control, lr
	mov lr, #0xffffffe9
	dmb
	cpsie if
	bx lr

_syscallend:
	cpsid if
	pop {lr}

	ldmia sp!, {r1-r8}
	mov r1, r0 /* Put result to user's r0 */
	stmdb lr!, {r1-r8}
	msr psp, lr

	ldmia sp!, {r4-r8}
	cpsie if

	/* Switch stacks */
	mov r0, #7
	msr control, r0

	ldmia sp!, {r0-r3, r12, lr}
	ldr pc, [sp], #8
.size _syscall_dispatch, .-_syscall_dispatch
.ltorg

.globl _exceptions_dispatch
.type _exceptions_dispatch, %function

_exceptions_dispatch:
	cpsid if

	mrs r0, psp
	stmdb sp!, {r0, r4-r11, lr}

	mrs r0, ipsr
	mov r1, sp
	b exceptions_dispatch
.size _exceptions_dispatch, .-_exceptions_dispatch
.ltorg

.globl _pendsv
.type _pendsv, %function
_pendsv:
	cpsid if

	vstmdb sp!, {s16-s31}
	mrs r0, ipsr
	mrs r3, psp
	sub r1, sp, #48
	ldr r2, =0xe000ef38
	ldr r2, [r2]
	stmdb sp!, {r1-r11, lr}

	ldr r5, =_cpu_nctx
	ldr r6, [r5]
	cmp r6, #0
	beq _intd0
	mov sp, r6
	mov r6, #0
	str r6, [r5]
	b _intd1
.size _pendsv, .-_pendsv
.ltorg

.globl _interrupts_dispatch
.type _interrupts_dispatch, %function
_interrupts_dispatch:
	cpsid if

	vstmdb sp!, {s16-s31}
	mrs r0, ipsr
	mrs r3, psp
	sub r1, sp, #48
	ldr r2, =0xe000ef38
	ldr r2, [r2]
	stmdb sp!, {r1-r11, lr}

_intd0:
	bl interrupts_dispatch
	ldr sp, [sp]
_intd1:
	ldmia sp!, {r1-r11, lr}
	vldmia sp!, {s16-s31}

	ldr r1, =0xe000ef38
	str r2, [r1]

	msr psp, r3

	/* Check if we're returning to userspace */
	and r1, lr, #4
	ror r1, r1, #2
	orr r1, r1, #(1 << 2)
	msr control, r1

	dmb
	cpsie if
	bx lr
.size _interrupts_dispatch, .-_interrupts_dispatch
.ltorg


.globl _hal_invokePendSV
.type _hal_invokePendSV, %function
_hal_invokePendSV:
	mov r1, #(1 << 28)
	ldr r2, =ADDR_SCB
	str r1, [r2, #4]
	bx lr
.size _hal_invokePendSV, .-_hal_invokePendSV
.ltorg


.globl hal_cpuReschedule
.type hal_cpuReschedule, %function
hal_cpuReschedule:
	push {r0, lr}
	eor r0, r0, r0 /* default return value */
	bl _hal_invokePendSV
	pop {r1, lr}
	cmp r1, #NULL
	it eq
	beq hal_cpuReschedule0
	add r1, r1, #12
	ldrexb r2, [r1]
	add r2, r2, #1
	dmb
	strexb r3, r2, [r1]
	ldrb r2, [r1, #1]
	msr primask, r2
hal_cpuReschedule0:
	isb
	dmb
	bx lr
.size hal_cpuReschedule, .-hal_cpuReschedule
.ltorg


.globl hal_jmp /* void hal_jmp(void *f, void *kstack, void *stack, int argc) */
.type hal_jmp, %function
hal_jmp:
	push {r0-r3}
	pop {r4-r7}

	cpsid if

	cmp r2, #NULL
	bne hal_jmp_user

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
1:
	cpsie if
	bx r4

hal_jmp_user:
	cpsid if
	msr msp, r5
	subs r7, #1
	bmi 2f
	pop {r0}
	subs r7, #1
	bmi 2f
	pop {r1}
	subs r7, #1
	bmi 2f
	pop {r2}
	subs r7, #1
	bmi 2f
	pop {r3}
2:
	cpsie if
	msr psp, r6
	mov r5, #0x3
	msr control, r5
	bx r4

.size hal_jmp, .-hal_jmp
.ltorg
