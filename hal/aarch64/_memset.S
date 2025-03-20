/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * hal_memset
 *
 * Copyright 2024 Phoenix Systems
 * Author: Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define VAL       x1
#define wVAL      w1
#define LEN       x2
#define DST_START x3
#define DST_END   x4
#define TMP       x5
#define TMP2      x6

/*
 * void *hal_memset(void *dst, int v, size_t l)
 * An efficient implementation of memset.
 * Uses unaligned accesses and DC ZVA instruction - may not work for uncached memory.
 * Doesn't use SIMD.
 *
 * The bottom byte of `v` is replicated by multiplying it with 0x0101010101010101.
 *
 * For 16 <= `len` < 64 bytes, stp is used to make 4 writes of 16 bytes.
 * For  4 <= `len` < 16 bytes, str is used to make 4 writes of 4 bytes.
 * For `len` < 4 bytes, str is used to make 3 writes of 1 byte.
 * Note that writes may overlap.
 *
 * For 64 <= `len` < 128, a loop is used to store 64 bytes per iteration without alignment.
 * For `len` >= 128, the first up to 64 bytes are stored without alignment, then a loop is used
 * to store 64 bytes per iteration with alignment. If `v` is 0, the loop uses `dc zva` cache
 * maintenance instruction to fill 64 bytes with zeroes.
 * For 2 cases above, bytes after the loop ends are handled by jumping into code for the first 3 cases.
 */
.globl hal_memset
.type hal_memset, %function
hal_memset:
.cfi_startproc
	mov DST_START, x0 /* Preserve original pointer in x0 */
	and VAL, VAL, #0xff
	mov TMP, 0x0101010101010101
	mul VAL, VAL, TMP

	cmp LEN, #64
	b.hs .Lmemset_large
.Lmemset_tail:
	add DST_END, DST_START, LEN
	cmp LEN, #16
	b.lo .Ltail15
.Ltail63:
	and TMP, LEN, #0x20
	sub TMP2, DST_END, TMP, lsr #1
	add TMP, DST_START, TMP, lsr #1
	stp VAL, VAL, [DST_START]
	stp VAL, VAL, [TMP]
	stp VAL, VAL, [TMP2, -16]
	stp VAL, VAL, [DST_END, -16]
	ret

.Ltail15:
	cmp LEN, #4
	b.lo .Ltail3
	and TMP, LEN, #0x8
	sub TMP2, DST_END, TMP, lsr #1
	add TMP, DST_START, TMP, lsr #1
	str wVAL, [DST_START]
	str wVAL, [TMP]
	str wVAL, [TMP2, -4]
	str wVAL, [DST_END, -4]
	ret

.Ltail3:
	cbz LEN, .Lreturn
	lsr LEN, LEN, #1
	strb wVAL, [DST_START]
	strb wVAL, [DST_START, LEN]
	strb wVAL, [DST_END, -1]
.Lreturn:
	ret

.Lmemset_large:
	cmp LEN, #128
	b.lo .Lnon_zva_loop        /* If smaller than 2 cache lines, we are not guaranteed to hit one whole cache line */
	ands TMP2, DST_START, 0x3f
	b.eq .Lcheck_zva
	sub TMP2, TMP2, 64         /* TMP2 is now negative of the number of bytes left */
	stp VAL, VAL, [DST_START]
	stp VAL, VAL, [DST_START, #16]
	stp VAL, VAL, [DST_START, #32]
	stp VAL, VAL, [DST_START, #48]
	sub DST_START, DST_START, TMP2
	add LEN, LEN, TMP2
.Lcheck_zva:
#ifndef MEMSET_WITHOUT_ZVA
	cbnz wVAL, .Lnon_zva_loop
#ifndef SKIP_ZVA_SIZE_CHECK
	mrs	TMP, dczid_el0
	and	TMP, TMP, 0x1f
	cmp	TMP, 4		/* ZVA size is 64 bytes.  */
	b.ne	.Lnon_zva_loop
#endif
.Lzva_loop:
	subs LEN, LEN, #64
	b.lo .Lprepare_tail
	dc zva, DST_START
	add DST_START, DST_START, #64
	b.hi .Lzva_loop
	ret

#endif
.Lnon_zva_loop:
	subs LEN, LEN, #64
	b.lo .Lprepare_tail
	stp VAL, VAL, [DST_START, #0]
	stp VAL, VAL, [DST_START, #16]
	stp VAL, VAL, [DST_START, #32]
	stp VAL, VAL, [DST_START, #48]
	add DST_START, DST_START, #64
	b.hi .Lnon_zva_loop
	ret

.Lprepare_tail:
	add LEN, LEN, #64
	b .Lmemset_tail

.cfi_endproc
.size hal_memset, .-hal_memset
.ltorg
