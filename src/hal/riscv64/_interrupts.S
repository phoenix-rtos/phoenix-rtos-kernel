/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt stubs
 *
 * Copyright 2018 Phoenix Systems
 * Author; Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "cpu.h"

.text

.macro SAVE
	csrc sstatus, 2
	addi sp, sp, -16

	j 1f

	/* If coming from userspace sscratch register holds kernel stack */
	csrrw tp, sscratch, tp
	beqz tp, 1f

	/* save user stack and task register in case of priv. switch */
	sd sp, (tp)
	csrr sp, sscratch
	ld sp, -8(tp)
	mv sp, tp
	addi sp, sp, -16
1:
	addi sp, sp, -272
	sd x1, (sp)          /* ra */
	sd x3, 8(sp)         /* gp */
	sd x5, 16(sp)        /* t0 */
	sd x6, 24(sp)        /* t1 */
	sd x7, 32(sp)        /* t2 */
	sd x8, 40(sp)        /* s0 */
	sd x9, 48(sp)        /* s1 */
	sd x10, 56(sp)       /* a0 */
	sd x11, 64(sp)       /* a1 */
	sd x12, 72(sp)       /* a2 */
	sd x13, 80(sp)       /* a3 */
	sd x14, 88(sp)       /* a4 */
	sd x15, 96(sp)       /* a5 */
	sd x16, 104(sp)      /* a6 */
	sd x17, 112(sp)      /* a7 */
	sd x18, 120(sp)      /* s2 */
	sd x19, 128(sp)      /* s3 */
	sd x20, 136(sp)      /* s4 */
	sd x21, 144(sp)      /* s5 */
	sd x22, 152(sp)      /* s6 */
	sd x23, 160(sp)      /* s7 */
	sd x24, 168(sp)      /* s8 */
	sd x25, 176(sp)      /* s9 */
	sd x26, 184(sp)      /* s10 */
	sd x27, 192(sp)      /* s11 */
	sd x28, 200(sp)      /* t3 */
	sd x29, 208(sp)      /* t4 */
	sd x30, 216(sp)      /* t5 */
	sd x31, 224(sp)      /* t6 */

	mv s0, sp

	/* Disable user memory access and FPU */
//	li t0, SR_SUM | 0x6000
//	csrrc s1, sstatus, t0

	csrrc s1, sstatus, 2
	csrr s2, sepc
	csrr s3, sbadaddr
	csrr s4, scause

	sd s0, 232(sp)
	sd s1, 240(sp)
	sd s2, 248(sp)
	sd s3, 256(sp)
	sd s4, 264(sp)
	sd tp, 272(sp)

	/* Kernel mode entered */
	csrw sscratch, zero
.endm


.macro RESTORE

	/* Switch kernel stack */
	ld sp, 232(sp)
	ld tp, 272(sp)

	/* Kernel mode left */
//	csrw sscratch, tp

	ld a0, 240(sp)
	andi a0, a0, ~2
	csrw sstatus, a0

	ld a2, 248(sp)
	csrw sepc, a2

	ld x1, (sp)          /* ra */
	ld x3, 8(sp)         /* gp */
	ld x5, 16(sp)        /* t0 */
	ld x6, 24(sp)        /* t1 */
	ld x7, 32(sp)        /* t2 */
	ld x8, 40(sp)        /* s0 */
	ld x9, 48(sp)        /* s1 */
	ld x10, 56(sp)       /* a0 */
	ld x11, 64(sp)       /* a1 */
	ld x12, 72(sp)       /* a2 */
	ld x13, 80(sp)       /* a3 */
	ld x14, 88(sp)       /* a4 */
	ld x15, 96(sp)       /* a5 */
	ld x16, 104(sp)      /* a6 */
	ld x17, 112(sp)      /* a7 */
	ld x18, 120(sp)      /* s2 */
	ld x19, 128(sp)      /* s3 */
	ld x20, 136(sp)      /* s4 */
	ld x21, 144(sp)      /* s5 */
	ld x22, 152(sp)      /* s6 */
	ld x23, 160(sp)      /* s7 */
	ld x24, 168(sp)      /* s8 */
	ld x25, 176(sp)      /* s9 */
	ld x26, 184(sp)      /* s10 */
	ld x27, 192(sp)      /* s11 */
	ld x28, 200(sp)      /* t3 */
	ld x29, 208(sp)      /* t4 */
	ld x30, 216(sp)      /* t5 */
	ld x31, 224(sp)      /* t6 */

//	csrr s0, sstatus
	addi sp, sp, 272
	j 1f

	beqz tp, 1f
	addi sp, sp, 16
	ld tp, -8(sp)
	ld sp, (sp)

1:
	addi sp, sp, 16
//	li tp, 0
.endm


.macro CHECKSIGNAL
	mv a0, zero
	call _proc_sigwant
	beqz a0, 1f
	mv a0, sp
	call proc_sighandle
	/* Not reached */
1:
.endm


.global interrupts_handleintexc
.type interrupts_handleintexc, @function
interrupts_handleintexc:
.align 8
	csrc sstatus, 2
	SAVE
	mv a0, sp

	bge s4, zero, 2f
	
	call handler

	mv a0, zero
	mv a1, sp
	call interrupts_dispatchIRQ
	j 3f

//	call interrupts_checkSignal
2:
	mv a0, s4
	andi a0, a0, 0xf
	mv a1, sp
	call exceptions_dispatch
	j 2b
3:
	CHECKSIGNAL
	RESTORE
	sret

.size interrupts_handleintexc, .-interrupts_handleintexc


.global hal_cpuReschedule
hal_cpuReschedule:
	csrrc tp, sstatus, 2
	
	/* Change correctly status register */
	csrw sepc, ra

	SAVE

	ori tp, tp, 0x100        /* Set supervistor bit */
	andi t0, tp, 0x2         /* Test SPIE */
	sll t0, t0, 4
	andi tp, tp, ~0x22
	or tp, tp, t0

	/* Save modified flags in old context */
	sd tp, 240(sp)

	beqz a0, 1f

	/* Set spinlock */
	ld t0, 64(a0)            /* Modify saved status */
	ori t0, t0, 0x100
	andi t1, t0, 0x2
	sll t1, t1, 4
	andi t0, t0, ~2
	or t0, t0, t1
	sd t0, 240(sp)

	li t0, 1
	addi a0, a0, 56
	amoswap.w.rl t0, t0, (a0)
1:
	mv a0, zero
	mv a1, sp
	mv a2, zero
	call threads_schedule

	CHECKSIGNAL
	RESTORE
	sret
.size hal_cpuReschedule, .-hal_cpuReschedule


.global hal_longjmp
hal_longjmp:
	mv sp, a0
	sd a0, 232(sp)
	RESTORE
	sret
.size hal_longjmp, .-hal_longjmp