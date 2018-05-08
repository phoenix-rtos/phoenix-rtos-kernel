/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * String functions
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__


.globl hal_memcpy
.type hal_memcpy, @function
hal_memcpy:
	/* Preserve return value */
	move t6, a0
	sltiu a3, a2, 128
	bnez a3, 4f

	/* Use word-oriented copy only if low-order bits match */ \
	andi a3, t6, 7
	andi a4, a1, 7
	bne a3, a4, 4f

	/* Skip if already aligned */
	beqz a3, 2f

	/* Round to nearest double word-aligned address */
	andi a3, a1, ~7
	addi a3, a3, 8

	/* Handle initial misalignment */
	sub a4, a3, a1
1:
	lb a5, 0(a1);
	addi a1, a1, 1
	sb a5, 0(t6)
	addi t6, t6, 1
	bltu a1, a3, 1b

	/* Update count */
	sub a2, a2, a4
2:
	andi a4, a2, ~((16 * 8) - 1)
	beqz a4, 4f
	add a3, a1, a4
3:
	/* Copy 128-byte blocks of data using registers */ \
	ld a4, 0(a1)
	ld a5, 8(a1)
	ld a6, 16(a1)
	ld a7, 24(a1)
	ld t0, 32(a1)
	ld t1, 40(a1)
	ld t2, 48(a1)
	ld t3, 56(a1)
	ld t4, 64(a1)
	ld t5, 72(a1)
	sd a4, 0(t6)
	sd a5, 8(t6)
	sd a6, 16(t6)
	sd a7, 24(t6)
	sd t0, 32(t6)
	sd t1, 40(t6)
	sd t2, 48(t6)
	sd t3, 56(t6)
	sd t4, 64(t6)
	sd t5, 72(t6)
	ld a4, 80(a1)
	ld a5, 88(a1)
	ld a6, 96(a1)
	ld a7, 104(a1)
	ld t0, 112(a1)
	ld t1, 120(a1)
	addi a1, a1, 128;
	sd a4, 80(t6);
	sd a5, 88(t6)
	sd a6, 96(t6)
	sd a7, 104(t6)
	sd t0, 112(t6)
	sd t1, 120(t6)
	addi t6, t6, 128
	bltu a1, a3, 3b

	/* Update count */
	andi a2, a2, 127
4:
	/* Handle trailing misalignment */
	beqz a2, 6f
	add a3, a1, a2

	/* Use word-oriented copy if co-aligned to word boundary */ \
	or a5, a1, t6
	or a5, a5, a3
	andi a5, a5, 3
	bnez a5, 5f
7:
	lw a4, 0(a1)
	addi a1, a1, 4
	sw a4, 0(t6)
	addi t6, t6, 4
	bltu a1, a3, 7b
	ret
5:
	lb a4, 0(a1)
	addi a1, a1, 1
	sb a4, 0(t6)
	addi t6, t6, 1
	bltu a1, a3, 5b
6:
	ret

.size hal_memcpy, .-hal_memcpy


.globl hal_memset
.type hal_memset, @function
hal_memset:
	move t0, a0

	/* Defer to byte-oriented fill for small sizes */
	sltiu a3, a2, 16
	bnez a3, 4f

	/* Round to nearest aligned address */
	addi a3, t0, 7
	andi a3, a3, ~7
	beq a3, t0, 2f

	/* Handle initial misalignment */
	sub a4, a3, t0
1:
	sb a1, 0(t0)
	addi t0, t0, 1
	bltu t0, a3, 1b
	sub a2, a2, a4
2: 
	/* Broadcast value into all bytes */
	andi a1, a1, 0xff
	slli a3, a1, 8
	or a1, a3, a1
	slli a3, a1, 16
	or a1, a3, a1
	slli a3, a1, 32
	or a1, a3, a1

	/* Calculate end address */
	andi a4, a2, ~7
	add a3, t0, a4

	andi a4, a4, 31 * 8  /* Calculate remainder */
	beqz a4, 3f          /* Shortcut if no remainder */
	neg a4, a4
	addi a4, a4, 32 * 8  /* Calculate initial offset */

	/* Adjust start address with offset */
	sub t0, t0, a4

	/* Jump into loop body - assumes 32-bit instruction lengths */
	la a5, 3f
	srli a4, a4, 1
	add a5, a5, a4
	jr a5
3:
	sd a1, 0(t0)
	sd a1, 8(t0)
	sd a1, 16(t0)
	sd a1, 24(t0)
	sd a1, 32(t0)
	sd a1, 40(t0)
	sd a1, 48(t0)
	sd a1, 56(t0)
	sd a1, 64(t0)
	sd a1, 72(t0)
	sd a1, 80(t0)
	sd a1, 88(t0)
	sd a1, 96(t0)
	sd a1, 104(t0)
	sd a1, 112(t0)
	sd a1, 120(t0)
	sd a1, 128(t0)
	sd a1, 136(t0)
	sd a1, 144(t0)
	sd a1, 152(t0)
	sd a1, 160(t0)
	sd a1, 168(t0)
	sd a1, 176(t0)
	sd a1, 184(t0)
	sd a1, 192(t0)
	sd a1, 200(t0)
	sd a1, 208(t0)
	sd a1, 216(t0)
	sd a1, 224(t0)
	sd a1, 232(t0)
	sd a1, 240(t0)
	sd a1, 248(t0)
	addi t0, t0, 256
	bltu t0, a3, 3b

	/* Update count */
	andi a2, a2, 7  
4:
	/* Handle trailing misalignment */
	beqz a2, 6f
	add a3, t0, a2
5:
	sb a1, 0(t0)
	addi t0, t0, 1
	bltu t0, a3, 5b
6:
	ret
.size hal_memset, .-hal_memset
