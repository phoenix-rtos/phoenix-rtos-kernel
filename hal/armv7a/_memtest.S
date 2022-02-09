/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for iMX6ULL processor memory test
 *
 * Copyright 2018 Phoenix Systems
 * Author: Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#define SWAP(x)       (((x >> 24) & 0xff) | ((x << 8) & (0xff << 16)) | ((x >> 8) & (0xff << 8)) | ((x << 24) & (0xff << 24)))

.arm

.section .init, "ax"
.global _start
.type _start, %function

.org 0
_vectors:
	ldr pc, =_start
	ldr pc, =_start
	ldr pc, =_start
	ldr pc, =_start
	ldr pc, =_start
	ldr pc, =_start
	ldr pc, =_start
	ldr pc, =_start

.org 0x400, 0xff

/* program image header */
ivt:
	.word 0x402000d1                              /* hdr */
	.word _start                                  /* entry */
	.word 0                                       /* reserved 1 */
	.word dcd                                     /* dcd */
	.word boot_data                               /* boot_data */
	.word ivt                                     /* self */
	.word 0                                       /* csf */
	.word 0                                       /* reserved 2 */

boot_data:
	.word _vectors                                /* load address */
	.word __bss_start__ - ivt                     /* size - will be changed during image creation */
	.word 0                                       /* plugin */
	.word 0                                       /* reserved */

dcd:
	.byte 0xd2                                    /* Tag */
	.byte ((_start - dcd) >> 8) & 0xff            /* Overall size of DCD (high) */
	.byte (_start - dcd) & 0xff                   /* Overall size of DCD (low) */
	.byte 0x41                                    /* Version */

dcd_clock:
	.byte 0xcc                                    /* Write tag */
	.byte ((dcd_ddr - dcd_clock) >> 8) & 0xff     /* Size high byte */
	.byte (dcd_ddr - dcd_clock) & 0xff            /* Size low byte */
	.byte 0x1c                                    /* OR mask word */

	.word SWAP(0x021b0000)
	.word SWAP(0x80000000)

	/* Enable DDR clock */
	.word SWAP(0x020c4074)
	.word SWAP(0x0f300000)

dcd_ddr:
	.byte 0xcc                                    /* Write tag */
	.byte ((_start - dcd_ddr) >> 8) & 0xff        /* Size high byte */
	.byte (_start - dcd_ddr) & 0xff               /* Size low byte */
	.byte 0x04                                    /* Write words */

	/* Config IOMUX */
	.word SWAP(0x020e04B4)
	.word SWAP(0x000c0000)
	.word SWAP(0x020e04ac)
	.word SWAP(0x00000000)
	.word SWAP(0x020e027c)
	.word SWAP(0x00000030)
	.word SWAP(0x020e0250)
	.word SWAP(0x00000030)
	.word SWAP(0x020e024c)
	.word SWAP(0x00000030)
	.word SWAP(0x020e0490)
	.word SWAP(0x00000030)
	.word SWAP(0x020e0288)
	.word SWAP(0x00000030)
	.word SWAP(0x020e0270)
	.word SWAP(0x00000000)
	.word SWAP(0x020e0260)
	.word SWAP(0x00000030)
	.word SWAP(0x020e0264)
	.word SWAP(0x00000030)
	.word SWAP(0x020e04A0)
	.word SWAP(0x00000030)
	.word SWAP(0x020e0494)
	.word SWAP(0x00020000)
	.word SWAP(0x020e0280)
	.word SWAP(0x00000030)
	.word SWAP(0x020e0284)
	.word SWAP(0x00000030)
	.word SWAP(0x020e04b0)
	.word SWAP(0x00020000)
	.word SWAP(0x020e0498)
	.word SWAP(0x00000030)
	.word SWAP(0x020e04a4)
	.word SWAP(0x00000030)
	.word SWAP(0x020e0244)
	.word SWAP(0x00000030)
	.word SWAP(0x020e0248)
	.word SWAP(0x00000030)

	/* Config DDR control registers */
	.word SWAP(0x021b001c)
	.word SWAP(0x00008000)
	.word SWAP(0x021b0800)
	.word SWAP(0xa1390003)
	.word SWAP(0x021b080c)
	.word SWAP(0x00150019)
	.word SWAP(0x021b083c)
	.word SWAP(0x41550153)
	.word SWAP(0x021b0848)
	.word SWAP(0x40403a3e)
	.word SWAP(0x021b0850)
	.word SWAP(0x40402f2a)
	.word SWAP(0x021b081c)
	.word SWAP(0x33333333)
	.word SWAP(0x021b0820)
	.word SWAP(0x33333333)
	.word SWAP(0x021b082c)
	.word SWAP(0xf3333333)
	.word SWAP(0x021b0830)
	.word SWAP(0xf3333333)
	.word SWAP(0x021b08c0)
	.word SWAP(0x00944009)
	.word SWAP(0x021b08b8)
	.word SWAP(0x00000800)

	/* Config MMDC init */
	.word SWAP(0x021b0004)
	.word SWAP(0x0002002d)
	.word SWAP(0x021b0008)
	.word SWAP(0x1b333030)
	.word SWAP(0x021b000c)
	.word SWAP(0x676b52f3)
	.word SWAP(0x021b0010)
	.word SWAP(0xb66d0b63)
	.word SWAP(0x021b0014)
	.word SWAP(0x01ff00db)
	.word SWAP(0x021b0018)
	.word SWAP(0x00201740)
	.word SWAP(0x021b001c)
	.word SWAP(0x00008000)
	.word SWAP(0x021b002c)
	.word SWAP(0x000026d2)
	.word SWAP(0x021b0030)
	.word SWAP(0x006b1023)
	.word SWAP(0x021b0040)
	.word SWAP(0x00000047)
	.word SWAP(0x021b0000)
	.word SWAP(0x82180000)
	.word SWAP(0x021b0890)
	.word SWAP(0x00400000)
	.word SWAP(0x021b001c)
	.word SWAP(0x02008032)
	.word SWAP(0x021b001c)
	.word SWAP(0x00008033)
	.word SWAP(0x021b001c)
	.word SWAP(0x00048031)
	.word SWAP(0x021b001c)
	.word SWAP(0x15208030)
	.word SWAP(0x021b001c)
	.word SWAP(0x04008040)
	.word SWAP(0x021b0020)
	.word SWAP(0x00000800)
	.word SWAP(0x021b0818)
	.word SWAP(0x00000227)
	.word SWAP(0x021b0004)
	.word SWAP(0x0002552d)
	.word SWAP(0x021b0404)
	.word SWAP(0x00011006)
	.word SWAP(0x021b001c)
	.word SWAP(0x00000000)

_start:
	cpsid aif, #0x1f

	/* Disable caches */
	mrc p15, 0, r1, c1, c0, 0
	bic r1, r1, #(1 << 12)
	bic r1, r1, #(1 << 2)
	mcr p15, 0, r1, c1, c0, 0

	ldr sp, =0x0091fff8
	blx test_ddr
	b _start
.size _start, .-_start
