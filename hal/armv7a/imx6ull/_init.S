/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for iMX6ULL processor
 *
 * Copyright 2018, 2020-2021 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Maciej Purski, Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>
#include <arch/pmap.h>
#include "config.h"

.extern pmap_common
.extern syspage
.extern relOffs
.extern _end
.extern imx6ull_bootReason


#define ADDR_OCRAM    0x907000
#define ADDR_STACK    0x803ff000
#define ADDR_TTL1     (pmap_common - VADDR_KERNEL + ADDR_DDR)
#define ADDR_TTL2_K   (ADDR_TTL1 + 4 * SIZE_PAGE)
#define ADDR_TTL2_EXC (ADDR_TTL2_K + SIZE_PAGE)

#define ADDR_PROGS_BEGIN (_end + 4 * 1024 * 1024 - VADDR_KERNEL + ADDR_DDR)

#define VADDR_SYSPAGE (_end + SIZE_PAGE - 1)
#define VADDR_UART1   (VADDR_SYSPAGE + 2 * SIZE_PAGE)
#define VADDR_UART2   (VADDR_UART1 + SIZE_PAGE)
#define VADDR_GIC     (VADDR_UART2 + SIZE_PAGE)
#define VADDR_GPT     (VADDR_GIC + 4 * SIZE_PAGE)
#define VADDR_CCM     (VADDR_GPT + SIZE_PAGE)
#define SWAP(x)       (((x >> 24) & 0xff) | ((x << 8) & (0xff << 16)) | ((x >> 8) & (0xff << 8)) | ((x << 24) & (0xff << 24)))


/* API vectors */
#define HAB_API_VEC   0x100
#define ROM_API_VEC   0x180

.arm

.section .init, "ax"
.global _start
.type _start, %function

.org 0
init_vectors:
	ldr pc, =_start
	ldr pc, =_exception_undef
	ldr pc, =_syscalls_dispatch
	ldr pc, =_exception_prefetch
	ldr pc, =_exception_abort
	.word 0
	ldr pc, =_interrupts_dispatch
	ldr pc, =_interrupts_dispatch

syspage_data:

.org 0x400, 0x0

plugin_ivt:
	.word 0x402000d1                                    /* hdr */
	.word plugin - VADDR_KERNEL + ADDR_OCRAM            /* entry */
	.word 0                                             /* reserved 1 */
	.word dcd - VADDR_KERNEL + ADDR_OCRAM               /* dcd */
	.word plugin_boot_data - VADDR_KERNEL + ADDR_OCRAM  /* boot_data */
	.word plugin_ivt - VADDR_KERNEL + ADDR_OCRAM        /* self */
	.word 0                                             /* csf */
	.word 0                                             /* reserved 2 */

plugin_boot_data:
	.word ADDR_OCRAM                                    /* load address */
	.word dcd_end + 0x30 - VADDR_KERNEL                 /* plugin size */
	.word 1                                             /* plugin */
	.word 0                                             /* reserved */

.macro mov32, reg, val
	movw \reg, #:lower16:\val
	movt \reg, #:upper16:\val
.endm

plugin:
	stmdb sp!, {r4, r5, r6, r7, r8, lr}

	/* note: "ldr reg, =constant" pseudo instruction can't be used before
	 * the serial download check below - the literal pool will not be included in the plugin */

	/* init watchdog */
	mov32 r4, 0x20bc000
#if defined(WATCHDOG)
	/* 128 sec timeout, debug, the clock is already enabled */
	movw r5, #0xff36
#else
	/* we need to keep plugin section size the same regardless of
	 * WATCHDOG define, so feed it with the default value */
	movw r5, #0x0030
#endif
	strh r5, [r4]

	/* check if reset was caused by watch dog */
	mov32 r4, 0x20d8008
	ldr r5, [r4]
	tst r5, #0x1

	/* skip mdscr register config */
	beq mdscr_skip

	mov32 r4, 0x21b001c
	mov32 r5, 0x2008032
	str r5, [r4]
	movw r5, #0x8033
	str r5, [r4]
	mov32 r5, 0x48031
	str r5, [r4]
	mov32 r5, 0x15208030
	str r5, [r4]
	mov32 r5, 0x4008040
	str r5, [r4]
	mov32 r4, 0x21b0020
	movw r5, #0x800
	str r5, [r4]

mdscr_skip:
	mov32 r4, 0x21b0818
	mov32 r5, 0x227
	str r5, [r4]
	mov32 r4, 0x21b0004
	mov32 r5, 0x2552d
	str r5, [r4]
	mov32 r4, 0x21b0404
	mov32 r5, 0x11006
	str r5, [r4]
	mov32 r4, 0x21b001c
	mov r5, #0x0
	str r5, [r4]

	/* check first argument's address - anything below OCRAM means it is a serial download boot */
	mov r4, #0x900000
	cmp r0, r4
	bmi failsafe
	mov r4, r0
	mov r5, r1
	mov r6, r2
	ldr r1, =(boot_data - VADDR_KERNEL + ADDR_OCRAM)
	ldr r2, =(ADDR_OCRAM + (syspage_data - init_vectors))
	ldr r2, [r2]     /* syspage->hs.imgsz */
	ldr r3, [r1, #4] /* boot_data->size */
	/* Update image size based on data in syspage */
	str r2, [r1, #4]
	ldmia r1, {r2, r3, r7, r8}
	str r8, [sp, #-4]!
	str r7, [sp, #-4]!
	str r3, [sp, #-4]!
	str r2, [sp, #-4]!
	mov r1, #0
	stmdb sp!, {r1, r2}
	add r2, sp, #8
	add r1, sp, #4
	add r0, sp, #0

	ldr r8, =(ROM_API_VEC + 0x8) /* (void **dest_address, u32 *load_size, boot_data_t *boot_data) */
	ldr r8, [r8]
	blx r8

	/* check if image was loaded correctly */
	ldr r0, [sp, #4]
	ldr r2, [sp, #12]
	cmp r0, r2
	addne sp, #24
	bne failsafe

	/* set output values for boot ROM*/
	ldmia sp!, {r2, r3} /* start address and size */
	str r2, [r4]
	str r3, [r5]
	ldr r2, =(ivt - VADDR_KERNEL) /* ivt offset */
	str r2, [r6]
	add sp, #16
	mov r0, #1
	ldmia sp!, {r4, r5, r6, r7, r8, pc}

failsafe:
	mov r4, #(HAB_API_VEC + 0x28) /* suppress sending error to host app */
	ldr r4, [r4]
	blx r4
	mov r0, #0x0
	ldmia sp!, {r4, r5, r6, r7, r8, pc}

dcd:
	.byte 0xd2                                    /* Tag */
	.byte ((dcd_end - dcd) >> 8) & 0xff           /* Overall size of DCD (high) */
	.byte (dcd_end - dcd) & 0xff                  /* Overall size of DCD (low) */
	.byte 0x41                                    /* Version */

dcd_clock:
	.byte 0xcc                                    /* Write tag */
	.byte ((dcd_ddr - dcd_clock) >> 8) & 0xff     /* Size high byte */
	.byte (dcd_ddr - dcd_clock) & 0xff            /* Size low byte */
	.byte 0x1c                                    /* OR mask word */

	.word SWAP(0x021b0000)
	.word SWAP(0x80000000)

	/* Enable DDR and watchdog clock */
	.word SWAP(0x020c4074)
	.word SWAP(0x0f330000)

	/* AIPSTZ1,2,3 clocks */
	.word SWAP(0x020c4068)
	.word SWAP(0x00000005)
	.word SWAP(0x020c4080)
	.word SWAP(0x00040000)

	/* EPITs clocks */
	.word SWAP(0x020c406c)
	.word SWAP(0x00005000)

dcd_ddr:
	.byte 0xcc                                    /* Write tag */
	.byte ((dcd_end - dcd_ddr) >> 8) & 0xff       /* Size high byte */
	.byte (dcd_end - dcd_ddr) & 0xff              /* Size low byte */
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
dcd_end:

ivt:
	.word 0x402000d1                                    /* hdr */
	.word _start - VADDR_KERNEL + ADDR_DDR              /* entry */
	.word 0                                             /* reserved 1 */
	.word 0                                             /* dcd */
	.word boot_data - VADDR_KERNEL + ADDR_DDR           /* boot_data */
	.word ivt - VADDR_KERNEL + ADDR_DDR                 /* self */
	.word 0                                             /* csf */
	.word 0                                             /* reserved 2 */

boot_data:
	.word ADDR_DDR                                      /* load address */
	.word __bss_start__ - ivt                           /* size - will be changed during image creation */
	.word 0                                             /* plugin */
	.word 0

_cpy4:
	mov r3, #4
	str r1, [r0], #4
	add r1, r1, r2
	subs r3, #1
	bne _cpy4 + 4
	mov pc, lr


/* startup code */
_start:
	cpsid aif, #SYS_MODE

/* Temporary solution: code allowing skipping phoenix-rtos-loader */
	ldr r0, =(ADDR_DDR + syspage_data - init_vectors)
	ldr r0, [r0, #16] /* points to syspage->progs address (circular list) */
	mov r1, r0
	cmp r1, #0
	beq init_syspage

	/* Copy applications to _end + 4MB */
	ldr r4, =ADDR_PROGS_BEGIN
	ldr r9, =(SIZE_PAGE - 1)

update_apps:
	ldr r2, [r1, #8]  /* prog->start */
	ldr r3, [r1, #12] /* prog->end */

	/* Align destination address to SIZE_PAGE */
	add r4, r4, r9
	bic r4, r4, r9

	/* Set new start in prog->start */
	str r4, [r1, #8]

copy_app:
	ldr r5, [r2], #4
	str r5, [r4], #4
	cmp r2, r3
	blo copy_app

	/* Set new end in prog->end */
	str r4, [r1, #12]

	/* Take pointer to the next program in the list */
	ldr r1, [r1]

	cmp r1, r0
	bne update_apps

init_syspage:
	/* Assign syspage address to syspage variable */
	ldr r0, =syspage
	sub r0, r0, #VADDR_KERNEL
	add r0, r0, #ADDR_DDR
	ldr r1, =VADDR_KERNEL
	add r1, r1, #(syspage_data - init_vectors)
	str r1, [r0]

	/* Define offset on which pointers should be moved */
	ldr r0, =relOffs
	sub r0, r0, #VADDR_KERNEL
	add r0, r0, #ADDR_DDR
	ldr r1, =(VADDR_KERNEL - ADDR_DDR)
	str r1, [r0]
/* End of the the temporary code */

	/* Save and clear boot reason - RM: otherwise all boot reasons since POR */
	ldr r0, =0x20d8008
	ldr r7, [r0]    /* only bits 16:0 are used */
	mov r2, #0xff   /* RM: bits 7:0 are w1c, other are w0c */
	str r2, [r0]

	/* WARNING: R7 - can't touch this until MMU enable */

	/* Clear PERSIST_SECONDARY_BOOT bit to rollback to primary in case we fail to boot */
	ldr r0, =0x020d8044
	ldr r1, [r0]
	bic r2, r1, #(0xffffff) /* save upper 8 bits of SRC_GPR10 into boot reason */
	orr r7, r7, r2
	bic r1, r1, #(1 << 30)
	str r1, [r0]

	/* Enable PMU cycle counter */
	mrc p15, 0, r0, c9, c12, 0
	orr r0, #0x7
	mcr p15, 0, r0, c9, c12, 0
	mrc p15, 0, r0, c9, c12, 1
	orr r0, #1 << 31
	mcr p15, 0, r0, c9, c12, 1

	/* Enable SMP */
	mrc p15, 0, r1, c1, c0, 1
	orr r1, r1, #(1 << 6)
	mcr p15, 0, r1, c1, c0, 1


#ifndef CPU_IMX6UL
	/* Set ARM clock to 792 MHz */
	/* Set ARM clock divider to 1 */
	ldr r0, =0x20c4000
	mov r1, #0
	str r1, [r0, #0x10]
	dsb

	mov r2, #3
#else
	mov r2, #2
#endif

	/* Enable usermode device accesses */
	/* AIPSTZ1, 2, 3 */
	ldr r0, =0x0207c040
	mov r1, #0
aipstzl:
	str r1, [r0]
	str r1, [r0, #4]
	str r1, [r0, #8]
	str r1, [r0, #12]
	str r1, [r0, #16]
	add r0, r0, #0x00100000
	subs r2, r2, #1
	bne aipstzl

	/* Enable USB2 PLL (480 MHz) */
	ldr r0, =0x020c8020
	ldr r1, [r0]
	orr r1, r1, #0x3000
	str r1, [r0]
usb2_pll:
	ldr r1, [r0]
	ands r1, #(1 << 31) /* Check lock bit */
	beq usb2_pll

	ldr r1, [r0]
	/* Clear bypass */
	bic r1, r1, #(1 << 16)
	str r1, [r0]
	/* Set en_usb_clks */
	orr r1, r1, #(1 << 6)
	str r1, [r0]

	/* Enable ENETn PLL (both 50 MHz) */
	ldr r0, =0x020c80e0
	ldr r1, =0x102005
	str r1, [r0]

enet_pll:
	ldr r1, [r0]
	ands r1, #(1 << 31)
	beq enet_pll

	ldr r8, =(ADDR_DDR)


	/* Disable caches */
	mrc p15, 0, r1, c1, c0, 0
	bic r1, r1, #(1 << 12)
	bic r1, r1, #(1 << 2)
	mcr p15, 0, r1, c1, c0, 0

	/* Invalidate intruction cache */
	mov r1, #0
	mcr p15, 0, r1, c7, c5, 0

	/* Invalidate data cache */
	mrc p15, 1, r0, c0, c0, 0
	mov r3, #0x1ff
	and r0, r3, r0, lsr #13
	mov r1, #0
way_loop:
	mov r3, #0
set_loop:
	mov r2, r1, lsl #30
	orr r2, r3, lsl #5
	mcr p15, 0, r2, c7, c6, 2
	add r3, r3, #1
	cmp r0, r3
	bgt set_loop
	add r1, r1, #1
	cmp r1, #4
	bne way_loop

	/* Invalidate TLB */
	mcr p15, 0, r1, c8, c7, 0

	mrc p15, 0, r1, c1, c0, 0
	orr r1, r1, #(1 << 2)  /* Enable data cache */
	orr r1, r1, #(1 << 12) /* Enable instruction cache */
	orr r1, r1, #(1 << 11) /* Enable branch prediction */
	bic r1, r1, #(1 << 28) /* Disable TEX remap */
	mcr p15, 0, r1, c1, c0, 0
	dsb
	isb

	/* Init TTL1 */
	ldr r5, =ADDR_TTL1
	mov r1, #0
	mov r2, #(4096 * 6 - 4)
clear_ttl1:
	str r1, [r5, r2]
	subs r2, #4
	bne clear_ttl1
	str r1, [r5]

	/* Map 4 MB P 0x80000000 -> V 0x80000000 */
	add r0, r5, #((ADDR_DDR >> 20) << 2)
	ldr r1, =((ADDR_DDR & ~0xfffff) | 0x402)
	mov r2, #0x100000
	bl _cpy4


	/* Kernel TTL1 entries */
	/* map 4 MB P 0x80000000 -> V 0xc0000000 */
	add r0, r5, #((VADDR_KERNEL >> 20) << 2)
	ldr r1, =(ADDR_TTL2_K + 1)
	mov r2, #0x400
	bl _cpy4

	/* Exceptions vectors and stack TTL1 entry */
	ldr r0, =(ADDR_TTL1 + (0xffc << 2))
	ldr r1, =(ADDR_TTL2_EXC + 1)
	bl _cpy4

	ldr r8, =(ADDR_DDR)

	/* Exceptions vectors TTL2 entry */
	/* Map P 0x80000000 -> V 0xffff0000 */
	ldr r0, =(ADDR_TTL2_EXC + (0x3f0 << 2))
	orr r1, r8, #0x1a
	str r1, [r0]

	/* Stack TTL2 entry */
	/* Map P ADDR_STACK -> V 0xfffff000 */
	ldr r0, =(ADDR_TTL2_EXC + (0x3ff << 2))
	ldr r1, =((ADDR_STACK & ~0xfff) | 0x1e)
	str r1, [r0]

	/* Set vector table pointer */
	ldr r0, =0xffff0000
	mcr p15, 0, r0, c12, c0, 0

	/* Kernel TTL2 entries */
	ldr r0, =ADDR_TTL2_K
	ldr r1, =((ADDR_DDR & ~0xfff) + (1024 * SIZE_PAGE) | 0x1e)
	mov r2, #(4 * 1024)
kernel_ttl2:
	subs r2, r2, #4
	sub r1, #SIZE_PAGE
	str r1, [r0, r2]
	bne kernel_ttl2

	/* Kernel page directory */
	ldr r1, =(pmap_common - VADDR_KERNEL)
	add r0, r1, lsr #10
	add r1, r1, r8
	orr r1, r1, #0x1f
	mov r2, #0x1000
	bl _cpy4
	bl _cpy4
	bl _cpy4
	bl _cpy4

	/* Kernel page tables */
	bl _cpy4

	/* Map UART1 4 KB P 0x02020000 -> V CEIL(_end + 3 * SIZE_PAGE, SIZE_PAGE) */
	ldr r0, =(VADDR_UART1 - VADDR_KERNEL)
	lsr r0, #12
	lsl r0, #2
	ldr r1, =ADDR_TTL2_K
	add r0, r0, r1
	ldr r1, =0x02020012
	str r1, [r0], #4

	/* Map UART2 4KB P 0x021e8000 -> V CEIL(_end + 4 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x021e8012
	str r1, [r0], #4

	/* Map GIC 16 KB after UARTs */
	mrc p15, 4, r1, c15, c0, 0 /* Get GIC paddr */
	lsr r1, #16
	lsl r1, #16
	orr r1, r1, #0x12
	mov r2, #(1 << 12)
	bl _cpy4

	/* Map EPIT1 after GIC */
	ldr r1, =0x020d0012
	str r1, [r0], #4

	/* Map GPT1 after EPIT1 */
	ldr r1, =0x02098012
	str r1, [r0], #4

	/* Map CCM registers 4KB P 0x020c4000 -> V CEIL(_end + 11 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x020c4012
	str r1, [r0], #4

	/* Map CCM_ANALOG registers 4KB P 0x020c8000 -> V CEIL(_end + 12 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x020c8012
	str r1, [r0], #4

	/* Map IOMUX_SNVS registers 4KB P 0x02290000 -> V CEIL(_end + 13 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x02290012
	str r1, [r0], #4

	/* Map IOMUX registers 4KB P 0x020e0000 -> V CEIL(_end + 14 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x020e0012
	str r1, [r0], #4

	/* Map IOMUXC_GPR registers 4KB P 0x020e4000 -> V CEIL(_end + 15 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x020e4012
	str r1, [r0], #4

	/* Map WDOG1 registers 4KB P 0x020bc000 -> V CEIL(_end + 16 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x020bc012
	str r1, [r0], #4

	/* Map SRC registers 4KB P 0x020d8000 -> V CEIL(_end + 17 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x020d8012
	str r1, [r0]


	/* IOMUX for UART1*/
	ldr r0, =0x20e0084
	mov r1, #0
	str r1, [r0], #4
	str r1, [r0], #0xc

	/* IOMUX for UART2 TXD */
	str r1, [r0], #4
	str r1, [r0]

	/* Enable UART1 clock */
	ldr r0, =0x020c407c
	ldr r1, [r0]
	orr r1, r1, #(3 << 24)
	str r1, [r0]


	/* Initialize MMU */
	mov r1, #1
	mcr p15, 0, r1, c2, c0, 2
	ldr r1, =ADDR_TTL1
	orr r1, r1, #(1 | (1 << 6)) /* Inner cacheability */
	orr r1, r1, #(3 << 3) /* Outer cacheability */
	mcr p15, 0, r1, c2, c0, 0
	mcr p15, 0, r1, c2, c0, 1

	ldr r1, =0x55555555
	mcr p15, 0, r1, c3, c0, 0

	/* Enable MMU */
	mrc p15, 0, r1, c1, c0, 0
	orr r1, r1, #1
	mcr p15, 0, r1, c1, c0, 0
	dsb
	isb

	/* Setup initial SP */
	eor r0, r0

	/* FIQ mode stack */
	msr CPSR_c, #(FIQ_MODE | NO_INT)
	mov sp, r0
	sub r0, r0, #0x20

	/* IRQ mode stack */
	msr CPSR_c, #(IRQ_MODE | NO_INT)
	mov sp, r0
	sub r0, r0, #0x100

	/* Supervisor mode stack */
	msr CPSR_c, #(SVC_MODE | NO_INT)
	mov sp, r0
	sub r0, r0, #0x40

	/* Undefined mode stack */
	msr CPSR_c, #(UND_MODE | NO_INT)
	mov sp, r0
	sub r0, r0, #0x40

	/* Abort mode stack */
	msr CPSR_c, #(ABT_MODE | NO_INT)
	mov sp, r0
	sub r0, r0, #0x40

	/* System mode stack */
	msr CPSR_c, #(SYS_MODE | NO_INT)
	mov sp, r0

	/* Enable FPU */
	mrc p15, 0, r0, c1, c0, 2 /* Read CPACR into R0 */
	orr r0, r0, #0x00f00000	 /* enable CP10 and CP11 for PL0 and PL1 */
	mcr p15, 0, r0, c1, c0, 2 /* Write R0 to CPACR */
	vmrs r0, fpexc
	orr r0, r0, #0x40000000
	vmsr fpexc, r0

	/* Store boot reason */
	ldr r0, =imx6ull_bootReason
	str r7, [r0]

	ldr pc, =main
.size _start, .-_start

#include "../_interrupts.S"
#include "../_armv7a.S"
