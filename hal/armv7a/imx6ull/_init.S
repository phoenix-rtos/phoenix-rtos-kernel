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

.macro dcd_addr_val, addr, val
	.word SWAP(\addr)
	.word SWAP(\val)
.endm

/* API vectors */
#define HAB_API_VEC   0x100
#define ROM_API_VEC   0x180

.arm

.section .init, "ax"
.global _start
.type _start, %function

.org 0
init_vectors:
	b  _start
	b  _exception_undef
	b  _syscalls_dispatch
	b  _exception_prefetch
	b  _exception_abort
	.word 0
	b  _interrupts_dispatch
	b  _interrupts_dispatch


#ifndef KERNEL_PLO_BOOT
/* Reserve 8 words in case we need a relocatable vector table in the future */
.org 0x40
syspage_data:
	.zero 0x3c0
syspage_end:

/* For NAND boot ROM will look for the IVT at offset 0x400 */
.org 0x400
ivt:
	.word 0x402000d1                                    /* hdr */
	.word _start - VADDR_KERNEL + ADDR_DDR              /* entry */
	.word 0                                             /* reserved 1 */
	.word dcd - VADDR_KERNEL + ADDR_DDR                 /* dcd */
	.word boot_data - VADDR_KERNEL + ADDR_DDR           /* boot_data */
	.word ivt - VADDR_KERNEL + ADDR_DDR                 /* self */
	.word 0                                             /* csf - should be filled in by external tools */
	.word 0                                             /* reserved 2 */

boot_data:
	.word ADDR_DDR                                      /* load address */
	.word _end - VADDR_KERNEL                           /* size (kernel without syspage) */
	.word 0                                             /* plugin */
	.word 0

/* SDP scripts assume DCD table exists at 0x440 */
.org 0x440
dcd:
	.byte 0xd2                                    /* Tag */
	.byte ((dcd_end - dcd) >> 8) & 0xff           /* Overall size of DCD (high) */
	.byte (dcd_end - dcd) & 0xff                  /* Overall size of DCD (low) */
	.byte 0x41                                    /* Version */

.Ldcd_clock:
	.byte 0xcc                                          /* Write tag */
	.byte ((.Ldcd_clock_end - .Ldcd_clock) >> 8) & 0xff /* Size high byte */
	.byte (.Ldcd_clock_end - .Ldcd_clock) & 0xff        /* Size low byte */
	.byte 0x1c                                          /* OR mask word */

	/* Enable DDR and watchdog clock */
	dcd_addr_val 0x020c4074 0x0f330000

	/* AIPSTZ1,2,3 clocks */
	dcd_addr_val 0x020c4068 0x00000005
	dcd_addr_val 0x020c4080 0x00040000

	/* EPITs clocks */
	dcd_addr_val 0x020c406c 0x00005000
.Ldcd_clock_end:

.Ldcd_ddr:
	.byte 0xcc                                      /* Write tag */
	.byte ((.Ldcd_ddr_end - .Ldcd_ddr) >> 8) & 0xff /* Size high byte */
	.byte (.Ldcd_ddr_end - .Ldcd_ddr) & 0xff        /* Size low byte */
	.byte 0x04                                      /* Write words */


	/* Config IOMUX */
	dcd_addr_val 0x020e04b4 0x000c0000
	dcd_addr_val 0x020e04ac 0x00000000
	dcd_addr_val 0x020e027c 0x00000030
	dcd_addr_val 0x020e0250 0x00000030
	dcd_addr_val 0x020e024c 0x00000030
	dcd_addr_val 0x020e0490 0x00000030
	dcd_addr_val 0x020e0288 0x00000030
	dcd_addr_val 0x020e0270 0x00000000
	dcd_addr_val 0x020e0260 0x00000030
	dcd_addr_val 0x020e0264 0x00000030
	dcd_addr_val 0x020e04a0 0x00000030
	dcd_addr_val 0x020e0494 0x00020000
	dcd_addr_val 0x020e0280 0x00000030
	dcd_addr_val 0x020e0284 0x00000030
	dcd_addr_val 0x020e04b0 0x00020000
	dcd_addr_val 0x020e0498 0x00000030
	dcd_addr_val 0x020e04a4 0x00000030
	dcd_addr_val 0x020e0244 0x00000030
	dcd_addr_val 0x020e0248 0x00000030

	/* Disable both DDR modules to reset them in case of a warm reset */
	dcd_addr_val 0x021b0000 0x00000000

	/* Config DDR control registers */
	dcd_addr_val 0x021b001c 0x00008000
	dcd_addr_val 0x021b0800 0xa1390003
	dcd_addr_val 0x021b080c 0x00150019
	dcd_addr_val 0x021b083c 0x41550153
	dcd_addr_val 0x021b0848 0x40403a3e
	dcd_addr_val 0x021b0850 0x40402f2a
	dcd_addr_val 0x021b081c 0x33333333
	dcd_addr_val 0x021b0820 0x33333333
	dcd_addr_val 0x021b082c 0xf3333333
	dcd_addr_val 0x021b0830 0xf3333333
	dcd_addr_val 0x021b08c0 0x00944009
	dcd_addr_val 0x021b08b8 0x00000800

	/* Config MMDC init */
	dcd_addr_val 0x021b0004 0x0002002d
	dcd_addr_val 0x021b0008 0x1b333030
	dcd_addr_val 0x021b000c 0x676b52f3
	dcd_addr_val 0x021b0010 0xb66d0b63
	dcd_addr_val 0x021b0014 0x01ff00db
	dcd_addr_val 0x021b0018 0x00201740
	dcd_addr_val 0x021b001c 0x00008000
	dcd_addr_val 0x021b002c 0x000026d2
	dcd_addr_val 0x021b0030 0x006b1023
	dcd_addr_val 0x021b0040 0x00000047
	dcd_addr_val 0x021b0000 0x82180000
	dcd_addr_val 0x021b0890 0x00400000
	dcd_addr_val 0x021b001c 0x02008032
	dcd_addr_val 0x021b001c 0x00008033
	dcd_addr_val 0x021b001c 0x00048031
	dcd_addr_val 0x021b001c 0x15208030
	dcd_addr_val 0x021b001c 0x04008040
	dcd_addr_val 0x021b0020 0x00000800
	dcd_addr_val 0x021b0818 0x00000227
	dcd_addr_val 0x021b0004 0x0002552d
	dcd_addr_val 0x021b0404 0x00011006
	dcd_addr_val 0x021b001c 0x00000000
.Ldcd_ddr_end:

dcd_end:
#endif

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

#ifdef KERNEL_PLO_BOOT
	ldr r0, =(relOffs - VADDR_KERNEL + ADDR_DDR)
	ldr r1, =VADDR_SYSPAGE
	lsr r1, #12
	lsl r1, #12

	sub r2, r1, r9
	str r2, [r0]

	ldr r0, [r9, #4] /* syspage size address - syspage address + sizeof(hal_syspage_t) */
	add r2, r9, r0   /* end of the syspage */

	ldr r0, =(syspage - VADDR_KERNEL + ADDR_DDR)
	str r1, [r0]

	add r1, r1, #(ADDR_DDR - VADDR_KERNEL)

syspage_cpy:
	ldr r3, [r9], #4
	str r3, [r1], #4
	cmp r9, r2
	blo syspage_cpy
#else
	/* init watchdog */
	ldr r0, =0x20bc000
#if defined(WATCHDOG)
	/* 128 sec timeout, debug, the clock is already enabled */
	movw r1, #0xff36
#else
	/* Disable watchdog (default value) */
	movw r1, #0x0030
#endif
	strh r1, [r0]

	ldr r0, =(ADDR_DDR + syspage_data - VADDR_KERNEL)
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
	ldr r0, =(syspage - VADDR_KERNEL + ADDR_DDR)
	ldr r1, =syspage_data
	str r1, [r0]

	/* Define offset on which pointers should be moved */
	ldr r0, =(relOffs - VADDR_KERNEL + ADDR_DDR)
	ldr r1, =(VADDR_KERNEL - ADDR_DDR)
	str r1, [r0]
#endif

	/* Save and clear boot reason - RM: otherwise all boot reasons since POR */
	ldr r0, =0x20d8000
	ldr r7, [r0, #0x8]    /* only bits 16:0 are used */
	mov r2, #0xff   /* RM: bits 7:0 are w1c, other are w0c */
	str r2, [r0, #0x8]

	/* WARNING: R7 - can't touch this until MMU enable */

	/* Clear PERSIST_SECONDARY_BOOT bit to rollback to primary in case we fail to boot */
	ldr r1, [r0, #0x44]
	bic r2, r1, #(0xffffff) /* save upper 8 bits of SRC_GPR10 into boot reason */
	orr r7, r7, r2
	bic r1, r1, #(1 << 30)
	str r1, [r0, #0x44]

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

	/* Set ARM clock to 792 MHz */
	/* Set ARM clock divider to 1 */
	ldr r0, =0x20c4000
	mov r1, #0
	str r1, [r0, #0x10]
	dsb

	mov r2, #3

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
	ldr r0, =0x020c8000
	ldr r1, [r0, #0x20]
	orr r1, r1, #0x3000
	str r1, [r0, #0x20]
usb2_pll:
	ldr r1, [r0, #0x20]
	ands r1, #(1 << 31) /* Check lock bit */
	beq usb2_pll

	ldr r1, [r0, #0x20]
	/* Clear bypass */
	bic r1, r1, #(1 << 16)
	str r1, [r0, #0x20]
	/* Set en_usb_clks */
	orr r1, r1, #(1 << 6)
	str r1, [r0, #0x20]

	/* Enable ENETn PLL (both 50 MHz) */
	ldr r1, =0x102005
	str r1, [r0, #0xe0]

enet_pll:
	ldr r1, [r0, #0xe0]
	ands r1, #(1 << 31)
	beq enet_pll

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

	/* Stack TTL2 entry */
	/* Map P ADDR_STACK -> V 0xfffff000 */
	ldr r0, =(ADDR_TTL2_EXC + (0x3ff << 2))
	ldr r1, =((ADDR_STACK & ~0xfff) | 0x1e)
	str r1, [r0]

	/* Set vector table pointer */
	ldr r0, =init_vectors
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
	ldr r8, =(ADDR_DDR)
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

#include "hal/armv7a/_interrupts.S"
#include "hal/armv7a/_armv7a.S"
