/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt stubs
 *
 * Copyright 2018, 2020, 2023, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Julia Kosowska, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

.extern hal_cpuKernelStack
.extern hal_multilock

.text

.macro MULTILOCK_CLEAR
	la t0, hal_multilock
	fence rw, w
	amoswap.w.rl zero, zero, (t0)
.endm


.macro SAVE
	csrrw a1, sscratch, a1  /* a1 = &hal_riscvHartData[hartId] */
	sd a0, 16(a1)           /* Save a0 */

	/* Read sstatus to a0 */
	csrr a0, sstatus

	/* Determine in which mode we were executing before interrupt
	 * SPP = 0 -> user mode, 1 -> kernel mode
	 */
	andi a0, a0, 0x100
	beqz a0, 1f

	/* Kernel mode */
	sd sp, -272(sp)

	addi sp, sp, -CPU_CTX_SIZE
	j 2f

1:
	/* User mode
	 * Load kernel stack pointer from hal_cpuKernelStack
	 */
	ld a0, 8(a1)

	/* Save task's stack pointer */
	sd sp, -272(a0)

	/* Swap to kernel stack */
	addi sp, a0, -CPU_CTX_SIZE

2:
	/* restore a0, a1 */
	ld a0, 16(a1)
	csrrw a1, sscratch, a1

	/* Save context */
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

	sd sp, 232(sp)       /* ksp */

	/* Check FPU status */
	csrr s1, sstatus
	srli t0, s1, 13

	/* If FPU is clean or dirty, save context */
	andi t0, t0, 2
	beqz t0, 3f

	/* Save FPU context */
	fsd f0, 296(sp)
	fsd f1, 304(sp)
	fsd f2, 312(sp)
	fsd f3, 320(sp)
	fsd f4, 328(sp)
	fsd f5, 336(sp)
	fsd f6, 344(sp)
	fsd f7, 352(sp)
	fsd f8, 360(sp)
	fsd f9, 368(sp)
	fsd f10, 376(sp)
	fsd f11, 384(sp)
	fsd f12, 392(sp)
	fsd f13, 400(sp)
	fsd f14, 408(sp)
	fsd f15, 416(sp)
	fsd f16, 424(sp)
	fsd f17, 432(sp)
	fsd f18, 440(sp)
	fsd f19, 448(sp)
	fsd f20, 456(sp)
	fsd f21, 464(sp)
	fsd f22, 472(sp)
	fsd f23, 480(sp)
	fsd f24, 488(sp)
	fsd f25, 496(sp)
	fsd f26, 504(sp)
	fsd f27, 512(sp)
	fsd f28, 520(sp)
	fsd f29, 528(sp)
	fsd f30, 536(sp)
	fsd f31, 544(sp)

	frcsr t0
	sd t0, 552(sp)

	/* Set FPU status to clean */
	li t0, SSTATUS_FS
	csrc sstatus, t0
	li t0, (2 << 13)
	csrs sstatus, t0
	csrr s1, sstatus
3:
	csrr s2, sepc
	csrr s4, scause

	sd s1, 240(sp)   /* sstatus */
	sd s2, 248(sp)   /* sepc */
	sd s4, 264(sp)   /* scause */
	sd tp, 280(sp)   /* tp */
.endm


.macro RESTORE
	ld a0, 240(sp)
	andi a0, a0, ~SSTATUS_SIE
	csrw sstatus, a0

	/* Check FPU status */
	srl t0, a0, 13
	/* If FPU is clean, restore context */
	andi t0, t0, 2
	beqz t0, 1f

	fld f0, 296(sp)
	fld f1, 304(sp)
	fld f2, 312(sp)
	fld f3, 320(sp)
	fld f4, 328(sp)
	fld f5, 336(sp)
	fld f6, 344(sp)
	fld f7, 352(sp)
	fld f8, 360(sp)
	fld f9, 368(sp)
	fld f10, 376(sp)
	fld f11, 384(sp)
	fld f12, 392(sp)
	fld f13, 400(sp)
	fld f14, 408(sp)
	fld f15, 416(sp)
	fld f16, 424(sp)
	fld f17, 432(sp)
	fld f18, 440(sp)
	fld f19, 448(sp)
	fld f20, 456(sp)
	fld f21, 464(sp)
	fld f22, 472(sp)
	fld f23, 480(sp)
	fld f24, 488(sp)
	fld f25, 496(sp)
	fld f26, 504(sp)
	fld f27, 512(sp)
	fld f28, 520(sp)
	fld f29, 528(sp)
	fld f30, 536(sp)
	fld f31, 544(sp)

	ld t0, 552(sp)
	fscsr t1, t0

1:
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

	ld tp, 280(sp)       /* tp */

	/* Restore task's stack pointer */
	ld sp, 288(sp)
.endm


.global _interrupts_dispatch
.type _interrupts_dispatch, @function
_interrupts_dispatch:
.align 8
	/* Disable interrupts */
	csrc sstatus, SSTATUS_SIE

	SAVE
	mv a1, sp

	/* Check interrupt source */
	li s1, SCAUSE_INTR
	and s0, s4, s1
	andi a0, s4, 0xff /* Exception code */
	beqz s0, _interrupts_notIrq

	/* IRQ */
	call interrupts_dispatch

	/* clear multilock only if rescheduled */
	bnez a0, _interrupts_return

	tail _interrupts_returnUnlocked

_interrupts_notIrq:
	li t1, SCAUSE_ECALL
	bne a0, t1, _interrupts_exception

	/* Syscall */
	addi s2, s2, 4 /* move pc past ecall instruction */
	sd s2, 248(sp)
	mv a0, a7      /* syscall number */
	ld a1, 288(sp) /* ustack */

	csrs sstatus, SSTATUS_SIE
	call syscalls_dispatch
	sd a0, 56(sp)
	csrc sstatus, SSTATUS_SIE

	tail _interrupts_returnUnlocked

_interrupts_exception:
	/* On some implementations (notably SPIKE), FPU instruction while FPU disabled
	 * causes illegal instruction exception. Opcode is checked against FPU instructions.
	 * If it is an FPU instruction, FPU is enabled for the thread.
	 */

	/* Check if illegal instruction */
	li t0, SCAUSE_ILLEGAL
	bne a0, t0, _interrupts_exceptionNotFpu

	/* Check if FPU disabled */
	ld t3, 240(sp)
	li t2, SSTATUS_FS
	and t2, t3, t2
	bnez t2, _interrupts_exceptionNotFpu

	/* Get failing instruction */
	csrr t2, stval
	andi t0, t2, 0x7f

	/* Check opcode:
	 * 0000111 - LOAD-FP
	 * 0100111 - STORE-FP
	 * 1000011 - MADD
	 * 1000111 - MSUB
	 * 1001011 - NMSUB
	 * 1001111 - NMADD
	 * 1010011 - OP-FP
	 */

	xori t1, t0, 0x53 /* OP-FP */
	beqz t1, _interrupts_exceptionFpu

	ori t1, t0, 0x20
	xori t1, t1, 0x27 /* LOAD/STORE-FP */
	beqz t1, _interrupts_exceptionFpu

	ori t1, t0, 0x0c
	xori t1, t1, 0x4f /* MADD/NMADD/MSUB/NMSUB */
	beqz t1, _interrupts_exceptionFpu

	/* Check FP CSRs.
	 * 1110011 - SYSTEM opcode
	 */

	xori t1, t0, 0x73
	bnez t1, 1f /* Not CSR, check compressed */

	/* EBREAK/ECALL check: funct3 */
	srli t1, t2, 12
	andi t1, t1, 0x7
	beqz t1, _interrupts_exceptionNotFpu

	/* Check CSR number */
	srli t1, t2, 20

	/* FP CSRs range: 0x001-0x003 */
	beqz t1, _interrupts_exceptionNotFpu
	andi t1, t1, ~0x3
	beqz t1, _interrupts_exceptionFpu

1:
	/* Compressed RV64 FP instructions: (16-bit)
	 * [funct3]...[op]
	 *     001 ... 00  c.fld
	 *     001 ... 10  c.fldsp
	 *     101 ... 00  c.fsd
	 *     101 ... 10  c.fsdsp
	 *
	 * If RV32 support is added, also check:
	 *     011 ... 00  c.flw
	 *     011 ... 10  c.flwsp
	 *     111 ... 00  c.fsw
	 *     111 ... 10  c.fswsp
	 *
	 * Noncompressed RV instructions have op[1:0] = 11.
	 * We are interested in op[1:0] = 00 | 10.
	 */

	/* op[0] */
	andi t1, t2, 0x1
	bnez t1, _interrupts_exceptionNotFpu

	/* funct3 */
	srli t2, t2, 13
	andi t2, t2, 0x7
	ori t2, t2, (1 << 2)
	xori t2, t2, 5
	bnez t2, _interrupts_exceptionNotFpu

_interrupts_exceptionFpu:
	/* Set `initial` FPU state - t3 hold previous sstatus */
	li t1, (1 << 13)
	csrs sstatus, t1
	or t3, t3, t1
	sd t3, 240(sp)

	fmv.d.x f0, zero
	fmv.d.x f1, zero
	fmv.d.x f2, zero
	fmv.d.x f3, zero
	fmv.d.x f4, zero
	fmv.d.x f5, zero
	fmv.d.x f6, zero
	fmv.d.x f7, zero
	fmv.d.x f8, zero
	fmv.d.x f9, zero
	fmv.d.x f10, zero
	fmv.d.x f11, zero
	fmv.d.x f12, zero
	fmv.d.x f13, zero
	fmv.d.x f14, zero
	fmv.d.x f15, zero
	fmv.d.x f16, zero
	fmv.d.x f17, zero
	fmv.d.x f18, zero
	fmv.d.x f19, zero
	fmv.d.x f20, zero
	fmv.d.x f21, zero
	fmv.d.x f22, zero
	fmv.d.x f23, zero
	fmv.d.x f24, zero
	fmv.d.x f25, zero
	fmv.d.x f26, zero
	fmv.d.x f27, zero
	fmv.d.x f28, zero
	fmv.d.x f29, zero
	fmv.d.x f30, zero
	fmv.d.x f31, zero

	tail _interrupts_returnUnlocked

_interrupts_exceptionNotFpu:
	csrr s3, sbadaddr
	/* Save sscratch to be able to get hart ID */
	csrr s5, sscratch
	sd s3, 256(sp)   /* sbadaddr */
	sd s5, 272(sp)

	mv a1, sp
	call exceptions_dispatch

	tail _interrupts_returnUnlocked
.size _interrupts_dispatch, .-_interrupts_dispatch


.global hal_cpuReschedule
.type hal_cpuReschedule, @function
hal_cpuReschedule:
	/* Disable interrupts */
	csrc sstatus, SSTATUS_SIE
	/* Set SPP to supervisor mode */
	li t0, SSTATUS_SPP
	csrs sstatus, t0

	/* Save return address */
	csrw sepc, ra

	SAVE

	/* Default return value (0) */
	sd zero, 56(sp)

	beqz a0, .LnoSpinlock

.Lspinlocked:
	/* Modify saved status */
	ld t0, (a1)
	ori t0, t0, SSTATUS_SPP
	sd t0, 240(sp)

	/* Save spinlock */
	mv s0, a0

	mv a0, zero
	mv a1, sp
	mv a2, zero
	call _threads_schedule

	/* Clear spinlock */
	addi s0, s0, 24
	fence
	amoswap.w.rl zero, zero, (s0)

	tail _interrupts_return

.LnoSpinlock:
	mv a0, zero
	mv a1, sp
	mv a2, zero
	call threads_schedule

	tail _interrupts_return
.size hal_cpuReschedule, .-hal_cpuReschedule


.type _interrupts_return, @function
_interrupts_return:
	/* Switch stack */
	ld sp, 232(sp)
	MULTILOCK_CLEAR

.Lmultilock_cleared:
	RESTORE
	sret

_interrupts_returnUnlocked:
	ld sp, 232(sp)
	j .Lmultilock_cleared
.size _interrupts_return, .-_interrupts_return


.global hal_jmp  /* void hal_jmp(void *f, void *kstack, void *ustack, int kargc, const arg_t *kargs) */
.type hal_jmp, @function
hal_jmp:
	csrc sstatus, SSTATUS_SIE /* disable interrupts */
	mv s0, a0
	mv s1, a1
	mv s2, a2
	mv s3, a3

	bnez a2, 2f

	mv sp, s1

	addi s3, s3, -1
	blt s3, zero, 1f
	ld a0, (a4)
	addi s3, s3, -1
	blt s3, zero, 1f
	ld a1, 8(a4)
	addi s3, s3, -1
	blt s3, zero, 1f
	ld a2, 16(a4)
	addi s3, s3, -1
	blt s3, zero, 1f
	ld a3, 24(a4)
1:
	csrs sstatus, SSTATUS_SIE /* enable interrupts */
	jr s0

2:
	mv sp, s2 /* user stack pointer */
	li t0, 0x120
	csrc sstatus, t0 /* sret will return to usermode */
	csrw sepc, s0 /* address to return to */

	fence.i
	sret
.size hal_jmp, .-hal_jmp
