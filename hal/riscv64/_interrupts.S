/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt stubs
 *
 * Copyright 2018, 2020 Phoenix Systems
 * Author; Pawel Pisarczyk, Julia Kosowska
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#define SR_FS           0x00006000
#define SR_SUM          0x00040000
#define SR_MER          0x00080000

#include "cpu.h"

.text

.macro SAVE
	csrc sstatus, 2
	addi sp, sp, -16

//	j 1f

	/* If coming from userspace sscratch register holds kernel stack */
	csrrw tp, sscratch, tp
	beqz tp, 1f

	/* save user stack and task register in case of priv. switch */
	sd sp, -8(tp)        /* usp */
	csrr sp, sscratch
	sd sp, -16(tp)       /* tp */
	mv sp, tp
	addi sp, sp, -16
1:
	addi sp, sp, -280
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

	/* Disable FPU */
	li t0, SR_FS
	csrc sstatus, t0
	/* Allow supervisor access to user space and reading from execute pages (for the time being) */
	li t0, SR_SUM | SR_MER
	csrs sstatus, t0

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
	csrw sscratch, tp

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
	addi sp, sp, 280
//	j 1f

	beqz tp, 1f
	addi sp, sp, 16
	ld tp, -16(sp)
	ld sp, -8(sp)

1:
	addi sp, sp, 16
//	li tp, 0
.endm


.macro CHECKSIGNAL
//	mv a0, zero
//	call _proc_sigwant
//	beqz a0, 1f
//	mv a0, sp
//	call proc_sighandle
	/* Not reached */
1:
.endm


.global interrupts_handleintexc
.type interrupts_handleintexc, @function
interrupts_handleintexc:
.align 8
	csrc sstatus, 2
	csrc sie, 8
	SAVE
	mv a0, sp

	li t1, 8
	beq s4, t1, 3f
	bge s4, zero, 2f

	li t1, 0x8000000000000009
	bne s4, t1, 33f
	li a0, 0xa
	j 34f

33:	
	li t1, 0x8000000000000005
	bne s4, t1, 34f
	call handler
	mv a0, zero
34:
	mv a1, sp
	call interrupts_dispatchIRQ
	j 4f

//	call interrupts_checkSignal
2:
	mv a0, s4
	andi a0, a0, 0xf
	mv a1, sp
	call exceptions_dispatch
	j 5f
3:
	addi s2, s2, 4 /* move pc past ecall instruction */
	sd s2, 248(sp)
	mv a0, a7
	ld a1, 288(sp)      /* usp */
	li t0, 16
	add a1, a1, t0
	call syscalls_dispatch
	sd a0, 56(sp)
	j 5f
4:
	beq a0, zero, 5f
	li a0, 0
	mv a1, sp
	call threads_schedule;

5:
	CHECKSIGNAL
	RESTORE
	sret

.size interrupts_handleintexc, .-interrupts_handleintexc


.global hal_cpuReschedule
hal_cpuReschedule:
	csrrc tp, sstatus, 2

	/* Change correctly status register */
	csrw sepc, ra

	beqz a0, 1f

	/* Set spinlock */
	ld t0, 64(a0)            /* Modify saved status */
	ori t0, t0, 0x100
	andi t1, t0, 0x2
	sll t1, t1, 4
	andi t0, t0, ~2
	or t0, t0, t1
//	sd t0, 240(sp)

	li t0, 1
	addi a0, a0, 56
	amoswap.w.rl t0, t0, (a0)
1:
	mv a0, zero

	SAVE

	ori tp, tp, 0x100        /* Set supervistor bit */
	andi t0, tp, 0x2         /* Test SPIE */
	sll t0, t0, 4
	andi tp, tp, ~0x22
	or tp, tp, t0

	/* Save modified flags in old context */
	sd tp, 240(sp)


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


.global hal_jmp  /* void hal_jmp(void *f, void *kstack, void *stack, int argc) */
hal_jmp:
       mv s0, a0
       mv s1, a1
       mv s2, a2
       mv s3, a3
       csrc sstatus, 2 /* disable interrupts */

       bnez a2, 2f

       mv sp, s1
       addi s3, s3, -1
       blt s3, zero, 1f
       ld a0, (sp)
       addi sp, sp, 8
       addi s3, s3, -1
       blt s3, zero, 1f
       ld a1, (sp)
       addi sp, sp, 8
       addi s3, s3, -1
       blt s3, zero, 1f
       ld a2, (sp)
       addi sp, sp, 8
       addi s3, s3, -1
       blt s3, zero, 1f
       ld a3, (sp)
       addi sp, sp, 8
1:
       csrs sstatus, 2 /* enable interrupts */
       jr s0

2:
       csrc sstatus, 2 /* disable interrupts */

       csrw sscratch, s1 /* kernel stack pointer */

       mv sp, s2 /* user stack pointer */
       addi sp, sp, 8 /* skip return address */

       addi s3, s3, -1
       blt s3, zero, 3f
       ld a0, (sp)
       addi sp, sp, 8
       addi s3, s3, -1
       blt s3, zero, 3f
       ld a1, (sp)
       addi sp, sp, 8
       addi s3, s3, -1
       blt s3, zero, 3f
       ld a2, (sp)
       addi sp, sp, 8
       addi s3, s3, -1
       blt s3, zero, 3f
       ld a3, (sp)
       addi sp, sp, 8
3:
	csrs sstatus, 2 /* enable interrupts */

       li t0, 0x100
       csrc sstatus, t0 /* sret will return to usermode */
       csrw sepc, s0 /* address to return to */

       sret

.size hal_jmp, .-hal_jmp