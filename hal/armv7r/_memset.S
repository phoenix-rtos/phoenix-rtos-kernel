/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * hal_memset
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define DST   r3
#define VAL_L r0
#define VAL_H r1
#define LEN   r2
#define TMP   r12

.thumb
.syntax unified

.section .init, "ax"
.align 4

/* void *hal_memset(void *dst, int v, size_t l)
 *
 * hal_memset implementation is divided into 2 parts:
 * 1. for len < 64 bytes
 * 2. for len >= 64 bytes
 *
 * For len < 64 bytes, possibly unaligned str instructions are used to store
 * 4 bytes at a time, and then the remaining 1-3 bytes are stored using strb.
 *
 * For len >= 64 bytes, first the destination address is aligned to 64 bytes,
 * and then if len is still >= 64 bytes, 64 bytes are stored at a time using
 * unrolled strd. At the end, if len is still >= 8 bytes, 8 bytes are stored
 * at a time using strd. Finally, for the last < 8 bytes, the same code as
 * for len < 64 bytes is used.
 */

.thumb_func

.globl hal_memset
.type hal_memset, %function
hal_memset:
	str r0, [sp, #-8]!
	mov DST, r0

	and VAL_H, #0xff
	orr VAL_H, VAL_H, VAL_H, lsl #8
	orr VAL_H, VAL_H, VAL_H, lsl #16

	/* quickly deal with < 64 byte len */
	cmp LEN, #64
	bhs .Lhal_memset64

.Ltail63:
	/* TMP = LEN / 4 */
	movs TMP, LEN, lsr #2
	beq .Ltail63_0

.Ltail63_4:
	str VAL_H, [DST], #4
	subs TMP, TMP, #1
	bne .Ltail63_4

.Ltail63_0:
	/* LEN = LEN % 4 */
	ands LEN, #3
	beq .Lreturn

1:
	strb VAL_H, [DST], #1
	subs LEN, LEN, #1
	bne 1b

.Lreturn:
	ldr r0, [sp], #8
	bx lr

.Lhal_memset64:
	/* at least 64 bytes to store */

	/* aligned to 64? */
	ands TMP, DST, #63
	beq .Laligned64

	rsb TMP, #64
	sub LEN, TMP
	cmp TMP, #4
	blo 3f

	sub TMP, #4
2:
	/* store 4 bytes at a time */
	str VAL_H, [DST], #4
	subs TMP, TMP, #4
	bhs 2b

3:
	/* 1-3 bytes to store */
	tst TMP, #2
	itt ne
	strbne VAL_H, [DST], #1
	strbne VAL_H, [DST], #1

	tst TMP, #1
	it ne
	strbne VAL_H, [DST], #1

	/* < 64 after aligning? */
	cmp LEN, #64
	blo .Ltail63

.Laligned64:
	/* at least 64 bytes to store, we're 64-byte aligned */
	mov VAL_L, VAL_H

	/* store 64 bytes at a time */
	sub LEN, LEN, #64
	sub DST, #8

4:
	strd VAL_L, VAL_H, [DST, #8]
	strd VAL_L, VAL_H, [DST, #16]
	strd VAL_L, VAL_H, [DST, #24]
	strd VAL_L, VAL_H, [DST, #32]
	strd VAL_L, VAL_H, [DST, #40]
	strd VAL_L, VAL_H, [DST, #48]
	strd VAL_L, VAL_H, [DST, #56]
	strd VAL_L, VAL_H, [DST, #64]!
	subs LEN, LEN, #64
	bhs 4b

	add DST, #8

	ands LEN, LEN, #63
	beq .Lreturn  /* return if len == 0 */
	cmp LEN, #8
	blo .Ltail63

.Lhal_memset8:
	/* at least 8 bytes to store, we're 8-byte aligned */

	/* store 8 bytes at a time */
	sub LEN, #8

5:
	strd VAL_L, VAL_H, [DST], #8
	subs LEN, LEN, #8
	bhs 5b

	/* make LEN positive again */
	ands LEN, #7
	beq .Lreturn

	b .Ltail63
.size hal_memset, .-hal_memset
