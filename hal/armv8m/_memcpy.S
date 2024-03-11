/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * hal_memcpy
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define DST r0
#define SRC r1
#define LEN r5

.thumb
.syntax unified

.section .init, "ax"
.align 4

/* void *hal_memcpy(void *dst, const void *src, size_t len)
 *
 *
 * Implementation is divided into the following cases:
 * 1. len < 64 bytes - copying is done without alignment checks,
 *    using possibly unaligned `ldr`, `str` instructions.
 * 2. src/dst mutually aligned to 8 bytes and len >= 64 bytes:
 *    unrolled `ldrd`, `strd` instructions are used to copy 64 bytes at a time.
 * 3. src/dst not mutually aligned, len >= 64 bytes:
 *    dst is brought to 64-byte alignment and then 64-byte chunks are copied
 *    using 2 unaligned `ldr` and aligned `strd` instructions.
 *
 * Long loops are aligned to 64 bytes.
 */

.thumb_func
.globl hal_memcpy
.type hal_memcpy, %function
hal_memcpy:
	push {r0, r5}
	mov LEN, r2
	cmp LEN, #64

	bhs .LblkCopy

	/* less than 64 bytes - always copy as if block was unaligned */

.Ltail63Unaligned:
	/* unaligned copy, 0-63 bytes */

	/* r3 = LEN / 4 */
	movs r3, LEN, lsr #2
	beq .Ltail63Un0

.Ltail63Un4:
	ldr r2, [SRC], #4
	str r2, [DST], #4
	subs r3, #1
	bne .Ltail63Un4

.Ltail63Un0:
	/* LEN = LEN % 4 */
	ands LEN, #3
	beq .Lreturn

	/* 1 <= LEN <= 3 */
1:
	ldrb r2, [SRC], #1
	strb r2, [DST], #1
	subs LEN, #1
	bne 1b

.Lreturn:
	pop {r0, r5}
	bx lr


.LblkCopyUnaligned64:
	/* src/dst not mutually aligned, more than 64 bytes to copy */

	/* align dst to 64 bytes */
	add r2, DST, #63
	bic r2, #63

	/* r2 = distance to 64-byte alignment */
	subs r2, DST
	beq 6f /* dst already aligned */

	str r4, [sp, #-8]!

	sub LEN, r2

	/* the same logic as in .Ltail63Unaligned */

	/* r3 = LEN / 4 */
	movs r3, r2, lsr #2
	beq 3f

2:
	/* align dst to 4 */
	ldr r4, [SRC], #4
	str r4, [DST], #4
	subs r3, #1
	bne 2b

3:
	/* r2 = r2 % 4 */
	ands r2, #3
	beq 5f

4:
	ldrb r3, [SRC], #1
	strb r3, [DST], #1
	subs r2, #1
	bne 4b

5:
	ldr r4, [sp], #8

	cmp LEN, #64
	blo .Ltail63Unaligned

6:
	sub LEN, #64

	sub DST, #8
	sub SRC, #8

.p2align 6
.LblkCopyUnaligned:
	/* copy block of 64 bytes */
	ldr r2, [SRC, #8]
	ldr r3, [SRC, #12]
	strd r2, r3, [DST, #8]

	ldr r2, [SRC, #16]
	ldr r3, [SRC, #20]
	strd r2, r3, [DST, #16]

	ldr r2, [SRC, #24]
	ldr r3, [SRC, #28]
	strd r2, r3, [DST, #24]

	ldr r2, [SRC, #32]
	ldr r3, [SRC, #36]
	strd r2, r3, [DST, #32]

	ldr r2, [SRC, #40]
	ldr r3, [SRC, #44]
	strd r2, r3, [DST, #40]

	ldr r2, [SRC, #48]
	ldr r3, [SRC, #52]
	strd r2, r3, [DST, #48]

	ldr r2, [SRC, #56]
	ldr r3, [SRC, #60]
	strd r2, r3, [DST, #56]

	ldr r2, [SRC, #64]!
	ldr r3, [SRC, #4]
	strd r2, r3, [DST, #64]!

	subs LEN, #64
	bhs .LblkCopyUnaligned

	add SRC, #8
	add DST, #8

	ands LEN, #63  /* make LEN positive again */
	beq .Lreturn   /* LEN = 0 */

	b .Ltail63Unaligned


.LblkCopy:
	/* copy more than 64 bytes */

	/* check src/dst alignment */
	and r3, SRC, #7
	and r2, DST, #7
	cmp r3, r2
	bne .LblkCopyUnaligned64

	/* src/dst mutually 8-byte aligned */

	/* handle leading misalignment, 1-3 bytes */
	ands r3, #3
	beq .LblkAligned4

	rsb r3, #4
	sub LEN, r3

7:
	ldrb r2, [SRC], #1
	strb r2, [DST], #1
	tst SRC, #3
	bne 7b

.LblkAligned4:
	ands r3, SRC, #7

	ittt ne
	/* leading misalignment aligned to 4 bytes */
	ldrne r2, [SRC], #4
	strne r2, [DST], #4
	subne LEN, LEN, #4

	/* src/dst aligned to 8 bytes */
	cmp LEN, #64
	blo .Ltail63Aligned

	pld [SRC]

	sub SRC, #8
	sub DST, #8

	sub LEN, #64

.p2align 6
.LblkCopy64:
	/* copy block of 64 bytes */
	pld [SRC, #40]

	ldrd r2, r3, [SRC, #8]
	strd r2, r3, [DST, #8]
	ldrd r2, r3, [SRC, #16]
	strd r2, r3, [DST, #16]
	ldrd r2, r3, [SRC, #24]
	strd r2, r3, [DST, #24]
	ldrd r2, r3, [SRC, #32]
	strd r2, r3, [DST, #32]

	pld [SRC, #72]

	ldrd r2, r3, [SRC, #40]
	strd r2, r3, [DST, #40]
	ldrd r2, r3, [SRC, #48]
	strd r2, r3, [DST, #48]
	ldrd r2, r3, [SRC, #56]
	strd r2, r3, [DST, #56]
	ldrd r2, r3, [SRC, #64]!
	strd r2, r3, [DST, #64]!
	subs LEN, #64
	bhs .LblkCopy64

	ands LEN, #63  /* make LEN positive again */
	beq .Lreturn   /* LEN = 0 */

	add SRC, #8
	add DST, #8

.Ltail63Aligned:
	/* copy the tail, 0-63 bytes
	 * src/dst are 8-byte aligned
	 */

	tst LEN, #(7 << 3)
	beq .Ltail63Al0

	sub LEN, #8

.Ltail63Al8:
	subs LEN, #8
	ldrd r2, r3, [SRC], #8
	strd r2, r3, [DST], #8
	bhs .Ltail63Al8

	and LEN, #7

.Ltail63Al0:
	/* copied all 8 byte aligned memory, now copy what's left */
	cmp LEN, #0
	beq .Lreturn

	/* 1 <= LEN <= 7 */
	tst LEN, #4
	itt ne
	ldrne r2, [SRC], #4
	strne r2, [DST], #4

	tst LEN, #2
	itt ne
	ldrhne r2, [SRC], #2
	strhne r2, [DST], #2

	tst LEN, #1
	itt ne
	ldrbne r2, [SRC], #1
	strbne r2, [DST], #1

	b .Lreturn

.size hal_memcpy, .-hal_memcpy
