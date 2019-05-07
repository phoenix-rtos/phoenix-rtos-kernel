/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for Cortex-M7 (ARMv7) architecture
 *
 * Copyright 2012, 2016-2017 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "cpu.h"

.syntax unified
.cpu cortex-m7
.thumb


.extern _cpu_nctx
.extern syscalls
.extern syspage


#define ADDR_ITCM 0x00000000
#define ADDR_DTCM 0x20000000
#define SWAP(x)   (((x >> 24) & 0xff) | ((x << 8) & (0xff << 16)) | ((x >> 8) & (0xff << 8)) | ((x << 24) & (0xff << 24)))


.section .init, "x"

_fcfb:
.word 0x42464346 /* Tag */
.word 0x56010400 /* Version */
.word 0
.byte 0x3        /* readSampleClkSrc */
.byte 0x3        /* dataHoldTime */
.byte 0x3        /* dataSetupTime */
.byte 0x3        /* columnAdressWidth */
.byte 0          /* deviceModeCfgEnable */
.byte 0
.byte 0, 0       /* waitTimeCfgCommnads */
.word 0          /* deviceModeSeq */
.word 0          /* deviceModeArg */
.byte 0          /* configCmdEnable */
.byte 0, 0, 0
.word 0, 0, 0, 0 /* configCmdSeqs */
.word 0, 0, 0, 0 /* cfgCmdArgs */
.word 0x00000059 /* controllerMiscOption */
.byte 0          /* deviceType */
.byte 0x8        /* sflashPadType */
.byte 0x8        /* serialClkFreq */
.byte 0x1        /* lutCustomSeqEnable */
.word 0, 0
.word 0x04000000 /* sflashA1Size */
.word 0          /* sflashA2Size */
.word 0          /* sflashB1Size */
.word 0          /* sflashB2Size */
.word 0          /* csPadSettingOverride */
.word 0          /* sclkPadSettingOverride */
.word 0          /* dataPadSettingOverride */
.word 0          /* dqsPadSettingOverride */
.word 0          /* timeoutInMs */
.word 0          /* commandInterval */
.word 0x0000000f /* dataValidTime */
.byte 0xf, 0     /* busyOffset */
.byte 0x1, 0     /* busyBitPolarity */

/* lookupTable */
.byte 0x59, 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
.byte 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x01, 0x00
.byte 0xa0, 0x87, 0x18, 0x8b, 0x10, 0x8f, 0x04, 0xa7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
.byte 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0xaa, 0x87, 0x00, 0x87, 0x05, 0x87, 0x00, 0x87, 0x70, 0x87
.byte 0xa0, 0x87, 0x18, 0x8b, 0x10, 0x8f, 0x0b, 0xb7, 0x04, 0xa7, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
.byte 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0xaa, 0x87, 0x00, 0x87, 0x05, 0x87, 0x00, 0x87, 0xaa, 0x87
.byte 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0x55, 0x87, 0x00, 0x87, 0x02, 0x87, 0x00, 0x87, 0x55, 0x87
.byte 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0xaa, 0x87, 0x00, 0x87, 0x05, 0x87, 0x00, 0x87, 0x80, 0x87
.byte 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0xaa, 0x87, 0x00, 0x87, 0x05, 0x87, 0x00, 0x87, 0xaa, 0x87
.byte 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0x55, 0x87, 0x00, 0x87, 0x02, 0x87, 0x00, 0x87, 0x55, 0x87
.byte 0x00, 0x87, 0x18, 0x8b, 0x10, 0x8f, 0x00, 0x87, 0x30, 0x87, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
.byte 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0xaa, 0x87, 0x00, 0x87, 0x05, 0x87, 0x00, 0x87, 0xa0, 0x87
.byte 0x00, 0x87, 0x18, 0x8b, 0x10, 0x8f, 0x80, 0xa3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
.byte 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0xaa, 0x87, 0x00, 0x87, 0x05, 0x87, 0x00, 0x87, 0x80, 0x87
.byte 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0xaa, 0x87, 0x00, 0x87, 0x05, 0x87, 0x00, 0x87, 0xaa, 0x87
.byte 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0x55, 0x87, 0x00, 0x87, 0x02, 0x87, 0x00, 0x87, 0x55, 0x87
.byte 0x00, 0x87, 0x00, 0x87, 0x00, 0x87, 0xaa, 0x87, 0x00, 0x87, 0x05, 0x87, 0x00, 0x87, 0x10, 0x87
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

/* lutCustomSeq */
.byte 0x00, 0x00, 0x00, 0x00, 0x02, 0x01, 0x00, 0x00, 0x02, 0x03, 0x00, 0x00, 0x04, 0x05, 0x00, 0x00
.byte 0x02, 0x09, 0x00, 0x00, 0x04, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
.byte 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00

.word 0x00000200 /* pageSize */
.word 0x00040000 /* sectorSize */
.word 0x00000001 /* ipCmdSerialClkFreq */

.org _fcfb + 512

_syspage:

.org _fcfb + 0x1000, 0x0

ivt:
	.word 0x402000d1                    /* hdr */
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
	.word 0

ivt_end:

.org _fcfb + 0x1100, 0x0

.globl _init_vectors
.type _init_vectors, %function

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
.word _interrupts_dispatch /* PendSV */
.word _interrupts_dispatch /* Systick */

.word _interrupts_dispatch /* IRQ0 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ8 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ16 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ24 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ32 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ40 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ48 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ56 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ64 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ72 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ80 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ88 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ96 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ104 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ112 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ120 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ128 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ136 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ144 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ152 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch /* IRQ160 */
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.word _interrupts_dispatch
.size _init_vectors, .-_init_vectors

.globl _start
.type _start, %function

_start:
	cpsid if

	/* Init vector table and stack pointer */
	ldr r0, =0xe000ed08
	ldr r1, =_init_vectors
	bic r1, #1
	str r1, [r0]
	ldr r0, [r1]
	msr msp, r0
	bl _imxrt_init

	ldr r0, =syspage
	ldr r1, =_syspage
	str r1, [r0]
	bl main
.size _start, .-_start

.globl _syscall_dispatch
.type _syscall_dispatch, %function

_syscall_dispatch:
	/* Find out which stack has arguments */
	tst lr, #4
	ite eq
	mrseq r0, msp
	mrsne r0, psp

	push {lr}

	/* Get syscall number and its function pointer */
	ldr r1, =syscalls
	ldr r2, [r0, #24]
	ldrb r2, [r2, #-2]
	lsl r2, #2
	ldr r1, [r1, r2]

	/* Prepare pseudo context */
	mov r2, #0x01000000
	push {r2} /* PSR */
	push {r1} /* PC */
	mov r2, _syscallend - _syscallrel - 1
	add r2, r2, pc
_syscallrel:
	push {r2} /* LR */
	mov r2, #0
	push {r2} /* R12 */
	push {r2} /* R3 */
	push {r2} /* R2 */
	push {r2} /* R1 */
	push {r0} /* R0 */

	/* Exit handler mode to kernel thread mode */
	mov lr, #0
	msr control, lr
	mov lr, #0xfffffff9
	cpsie if
	bx lr

_syscallend:
	/* There is still original context on stack and return value in r0 register */
	pop {lr}

	tst lr, #4
	ite eq
	moveq r1, #0
	movne r1, #3
	msr control, r1

	ldr r1, [sp, #24]
	orr r1, r1, #1
	str r1, [sp, #24]

	pop {r1} /* sic! */
	pop {r1}
	pop {r2}
	pop {r3}
	pop {r12}
	pop {lr}
	ldr pc, [sp], #8
.size _syscall_dispatch, .-_syscall_dispatch

.globl _exceptions_dispatch
.type _exceptions_dispatch, %function

_exceptions_dispatch:
	cpsid if

	push {r4-r11}
	mrs r0, psp
	push {r0}

	mrs r0, ipsr
	mov r1, sp
	b exceptions_dispatch
.size _exceptions_dispatch, .-_exceptions_dispatch

.globl _interrupts_dispatch
.type _interrupts_dispatch, %function

_interrupts_dispatch:
	cpsid if

	push {r4-r11}
	push {lr}
	mrs r0, psp
	push {r0}

	sub r1, sp, #4
	push {r1}                /* ctx->savesp = sp */

	mrs r0, ipsr
	cmp r0, #14
	bne _intd0

	ldr r5, =_cpu_nctx
	ldr r6, [r5]
	cmp r6, #0
	beq _intd0
	mov sp, r6
	mov r6, #0
	str r6, [r5]
	b _intd1

_intd0:
	bl interrupts_dispatch
	ldr sp, [sp]             /* sp = ctx->savesp */
_intd1:
	add sp, sp, #4

	pop {r0}
	msr psp, r0
	pop {lr}

	tst lr, #4
	ite eq
	moveq r4, #0
	movne r4, #1
	msr control, r4

	pop {r4-r11}

	cpsie if
	bx lr
.size _interrupts_dispatch, .-_interrupts_dispatch


.globl hal_cpuReschedule
.type hal_cpuReschedule, %function
hal_cpuReschedule:
	push {r0, lr}
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
