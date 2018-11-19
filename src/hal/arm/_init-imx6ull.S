/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for iMX6ULL processor
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "cpu.h"
#include "pmap.h"
#include "exceptions.h"
#include "interrupts.h"

.extern pmap_common
.extern syspage
.extern _end

#define ADDR_OCRAM    0x907000
#define ADDR_DDR      0x80000000
#define ADDR_STACK    0x803ff000
#define SIZE_OCRAM    68 * 1024
#define ADDR_TTL1     (pmap_common - VADDR_KERNEL + ADDR_DDR)
#define ADDR_TTL2_K   (ADDR_TTL1 + 4 * SIZE_PAGE)
#define ADDR_TTL2_EXC (ADDR_TTL2_K + SIZE_PAGE)
#define VADDR_UART1   (_end + SIZE_PAGE - 1)
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
	ldr pc, =_start
	ldr pc, =_exception_undef
	ldr pc, =_syscalls_dispatch
	ldr pc, =_exception_prefetch
	ldr pc, =_exception_abort
	.word 0
	ldr pc, =_interrupts_dispatch
	ldr pc, =_interrupts_dispatch

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

plugin:
	stmdb sp!, {r4, r5, r6, r7, r8, lr}

	/* check first argument's address - anything below OCRAM means it is a serial download boot */
	ldr r4, =#0x900000
	cmp r0, r4
	bmi failsafe
	mov r4, r0
	mov r5, r1
	mov r6, r2
	ldr r1, =(boot_data - VADDR_KERNEL + ADDR_OCRAM)
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
	ldr r4, =(HAB_API_VEC + 0x28) /* suppress sending error to host app */
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

	/* Enable DDR clock */
	.word SWAP(0x020c4074)
	.word SWAP(0x0f300000)

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

	/* Copy applications and update syspage */
	ldr r8, =(ADDR_DDR)
	ldr r9, =(SIZE_PAGE - 1)

	add r0, r8, #0x20 /* Syspage address */
	ldr r4, [r0, #0x8] /* Kernel offset */
	add r0, r0, r4 /* add kernel offset */

	ldr r1, [r0, #0x114] /* Number of programs */
	add r0, r0, #0x118 /* Now points to syspage->progs */
	add r2, r8, #0x400000 /* Destination */
	bic r2, r2, r6

apps_copy:
	ldr r3, [r0] /* Source */
	ldr r4, [r0, #4] /* Source end */

	/* New source begin */
	str r2, [r0]

	cmp r1, #0
	beq apps_copy_end

app_copy:
	ldr r5, [r3], #4
	str r5, [r2], #4
	cmp r3, r4
	blo app_copy

	/* New source end */
	str r2, [r0, #4]

	sub r1, r1, #1
	add r0, r0, #24

	/* Align destination to SIZE_PAGE */
	add r2, r2, r9
	bic r2, r2, r9

	b apps_copy

apps_copy_end:

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
	bl _cpy4

	/* Map UART1 4 KB P 0x02020000 -> V CEIL(_end, SIZE_PAGE) */
	ldr r0, =(VADDR_UART1 - VADDR_KERNEL)
	lsr r0, #12
	lsl r0, #2
	ldr r1, =ADDR_TTL2_K
	add r0, r0, r1
	ldr r1, =0x02020012
	str r1, [r0], #4

	/* Map UART2 4KB P 0x021e8000 -> V CEIL(_end + SIZE_PAGE, SIZE_PAGE) */
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

	/* Map CCM registers 4KB P 0x020c4000 -> V CEIL(_end + 9 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x020c4012
	str r1, [r0], #4

	/* Map CCM_ANALOG registers 4KB P 0x020c8000 -> V CEIL(_end + 10 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x020c8012
	str r1, [r0], #4

	/* Map IOMUX_SNVS registers 4KB P 0x02290000 -> V CEIL(_end + 11 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x02290012
	str r1, [r0], #4

	/* Map IOMUX registers 4KB P 0x020e0000 -> V CEIL(_end + 12 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x020e0012
	str r1, [r0], #4

	/* Map IOMUXC_GPR registers 4KB P 0x020e4000 -> V CEIL(_end + 13 * SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =0x020e4012
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
	ldr r0, =0xfffffff8

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

	ldr pc, =main
.size _start, .-_start


.globl _exceptions_dispatch
.type _exceptions_dispatch, %function
_exception_undef:
	cpsid if
	stmfd sp, {r0-r4}
	mov r0, #1
	mrs r3, spsr
	tst r3, #0x20
	subeq r2, lr, #4
	subne r2, lr, #2
	b _exceptions_dispatch

_exception_prefetch:
	cpsid if
	stmfd sp, {r0-r4}
	mov r0, #3
	sub r2, lr, #4
	b _exceptions_dispatch

_exception_abort:
	cpsid if
	stmfd sp, {r0-r4}
	mov r0, #4
	sub r2, lr, #8

_exceptions_dispatch:
	mrs r3, spsr
	sub r1, sp, #0x14
	mrc p15, 0, r4, c13, c0, 4
	cps #SYS_MODE
	tst r3, #0x0f
	movne r4, sp
	stmfd r4!, {r2}
	stmfd r4!, {r5-r14}
	mov sp, r4
	ldmfd r1, {r4-r8}
	push {r3-r8}
	mrc p15, 0, r1, c6, c0, 2
	push {r1}
	mrc p15, 0, r1, c5, c0, 1
	push {r1}
	mrc p15, 0, r1, c6, c0, 0
	push {r1}
	mrc p15, 0, r1, c5, c0, 0
	push {r1}
	sub r1, sp, #4
	push {r1}

	ldr lr, =exceptions_dispatch
	blx lr

	ldr sp, [sp]
	add sp, sp, #20

	pop {r11}
	pop {r0-r10}
	mov r12, sp
	ldr sp, [r12, #0x8]
	ldr lr, [r12, #0xc]
	cps #IRQ_MODE
	push {r11}
	ldr r11, [r12, #0x0]
	ldr lr, [r12, #0x10]
	push {lr}
	ldr r12, [r12, #0x4]
	rfefd sp!
.size _exceptions_dispatch, .-_exceptions_dispatch


.globl _hal_cpuRestoreCtx
.type _hal_cpuRestoreCtx, %function
_hal_cpuRestoreCtx:
	/* Restore fpu context */
	pop {r4}
	vmsr fpscr, r4
	vpop {d0-d15}
	vpop {d16-d31}

	pop {r11} /* r11 - apsr */
	pop {r0-r10}
	mov r12, sp /* r12 - points saved r11, r12, sp, lr, pc */
	ldr sp, [r12, #0x8]
	ldr lr, [r12, #0xc]
	cps #IRQ_MODE
	push {r11} /* cpsr saved, r11 free */
	ldr r11, [r12, #0x0]
	ldr lr, [r12, #0x10]
	push {lr}
	ldr r12, [r12, #0x4]
	rfefd sp! /* return from exception - pops pc and cpsr */
.size _hal_cpuRestoreCtx, .-_hal_cpuRestoreCtx


.globl hal_cpuReschedule
.type hal_cpuReschedule, %function
hal_cpuReschedule:
	cpsid if
	push {lr}
	stmfd sp, {r0-r14}
	sub sp, sp, #0x3c

	/* Default return value - EOK */
	mov r1, #0
	str r1, [sp]

	ldr r1, [sp, #0x34]
	add r1, #4
	str r1, [sp, #0x34]

	mrs r4, cpsr

	cmp r0, #NULL
	beq 1f

	add r0, #12

	/* Spinlock clear */
	ldrexb r1, [r0]
	add r1, r1, #1
	dmb
	strexb r2, r1, [r0]
	ldrb r1, [r0, #1]

	bic r4, #0xff
	and r1, #0xff
	orr r4, r4, r1
1:
	/* store CPSR with adjusted M and I flags */
	and r5, lr, #1 /* extract Thumb flag from LR address */
	orr r4, r4, r5, lsl #5
	bic r4, #0xc0
	push {r4}

	/* Store fpu context */
	vpush {d16-d31}
	vpush {d0-d15}
	vmrs r4,fpscr
	push {r4}

	sub r1, sp, #8
	push {r1}
	push {r1}

	blx threads_schedule

	ldr sp, [sp]

	mov r0, #0
	blx _proc_sigwant
	cmp r0, #0
	bne _hal_sighandle

	add sp, sp, #8

	b _hal_cpuRestoreCtx
.size hal_cpuReschedule, .-hal_cpuReschedule


.globl _interrupts_dispatch
.type _interrupts_dispatch, %function
_interrupts_dispatch:
	stmfd sp, {r0-r3}
	mrs r2, spsr
	sub r1, lr, #4
	sub r0, sp, #0x10
	/* fetch kernel thread SP from TPIDRPRW register */
	mrc p15, 0, r3, c13, c0, 4

	/* return to SYS mode with no interrupts */
	cpsie af, #SYS_MODE

	/* If exception was not taken in user mode, use curren stack
	 * to store context. Otherwise use preffered one from r3 */
	tst r2, #0x0f
	movne r3, sp

	/* save return address */
	stmfd r3!, {r1}

	/* store original r4-r14 registers as in hal_cpuReschedule()
	 * (original r0-r3 are still on exception stack) */
	stmfd r3!, {r4-r14}
	mov sp, r3

	/* fetch original r0-r3 from exception stack and store on local one
	 * including SPSR stored in current r3 */
	ldmfd r0, {r3-r6}
	push {r2-r6}

	/* Store fpu context */
	vpush {d16-d31}
	vpush {d0-d15}
	vmrs r4,fpscr
	push {r4}

	/* save SP on top of the stack and pass it as arg1 to IRQ handler (it is cpu_context_t *) */
	sub r1, sp, #8
	push {r1}
	push {r1}

	ldr r2, =VADDR_GIC + 0x2000 /* Interrupt Acknowledge register */
	lsr r2, r2, #12
	lsl r2, r2, #12
	ldr r4, [r2, #0x0c] /* r4[12:10] = CPU_ID of software interrupts, r4[9:0] = INT_ID */
	ldr r3, =0x03fe
	and r2, r3, r4
	cmp r2, r3 /* Check that INT_ID is not 1023 or 1022 (spurious interrupt) */
	beq 1f
	add r3, r3, #1

	/* pass INT_ID as arg0 (in r0) to the IRQ handler */
	and r0, r3, r4

	blx interrupts_dispatch

	ldr r0, =VADDR_GIC + 0x2000
	lsr r0, r0, #12
	lsl r0, r0, #12
	str r4, [r0, #0x10] /* Update End of Interrupt register with original value from ICCIAR */
1:
	ldr sp, [sp]

	mov r0, #0
	blx _proc_sigwant
	cmp r0, #0
	bne _hal_sighandle

	add sp, sp, #8

	b _hal_cpuRestoreCtx
.size _interrupts_dispatch, .-_interrupts_dispatch


.globl _hal_sighandle
.type _hal_sighandle, %function
_hal_sighandle:
	mov r0, sp
	sub sp, #8
	blx proc_sighandle
.size _hal_sighandle, .-_hal_sighandle


.globl _syscalls_dispatch
.type _syscalls_dispatch, %function
_syscalls_dispatch:
	stmfd sp, {r0-r4}^
	sub r1, sp, #0x14
	mrs r3, spsr
	mov r2, lr
	tst r3, #THUMB_STATE
	ldreq r0, [r2, #-4]
	biceq r0, r0, #0xff000000
	ldrneh r0, [r2, #-2]
	bicne r0, r0, #0xff00
	mrc p15, 0, r4, c13, c0, 4

	cpsie af, #SYS_MODE

	stmfd r4!, {r2}
	stmfd r4!, {r5-r14}
	mov r2, sp
	mov sp, r4
	ldmfd r1, {r4-r8}
	push {r3-r8}
	vpush {d16-d31}
	vpush {d0-d15}
	vmrs r4,fpscr
	push {r4}
	sub r1, sp, #8
	push {r1}
	push {r1}

	mov r1, r2

	cpsie if

	blx syscalls_dispatch

	cpsid if

	str r0, [sp, #272]

	ldr sp, [sp]

	mov r0, #0
	blx _proc_sigwant
	cmp r0, #0
	bne _hal_sighandle

	add sp, sp, #8

	b _hal_cpuRestoreCtx
.size _syscalls_dispatch, .-_syscalls_dispatch


.globl hal_cpuGetCycles
.type hal_cpuGetCycles, %function
hal_cpuGetCycles:
	mrc p15, 0, r1, c9, c13, 0
	str r1, [r0]
	bx lr
.size hal_cpuGetCycles, .-hal_cpuGetCycles


.globl hal_cpuInvalDataCache
.type hal_cpuInvalDataCache, %function
hal_cpuInvalDataCache:
	ldr r1, =SIZE_CACHE_LINE - 1
	bic r0, r0, r1
	mcr p15, 0, r0, c7, c6, 1
	bx lr
.size hal_cpuInvalDataCache, .-hal_cpuInvalDataCache


.globl hal_cpuFlushDataCache
.type hal_cpuFlushDataCache, %function
hal_cpuFlushDataCache:
	ldr r1, =SIZE_CACHE_LINE - 1
	bic r0, r0, r1
	mcr p15, 0, r0, c7, c14, 1
	bx lr
.size hal_cpuFlushDataCache, .-hal_cpuFlushDataCache


.globl hal_cpuCleanDataCache
.type hal_cpuCleanDataCache, %function
hal_cpuCleanDataCache:
	ldr r1, =SIZE_CACHE_LINE - 1
	bic r0, r0, r1
	mcr p15, 0, r0, c7, c11, 1
	bx lr
.size hal_cpuCleanDataCache, .-hal_cpuCleanDataCache


.globl hal_cpuInvalASID
.type hal_cpuInvalASID, %function
hal_cpuInvalASID:
	and r0, r0, #0xff
	mcr p15, 0, r0, c8, c7, 2
	bx lr
.size hal_cpuInvalASID, .-hal_cpuInvalASID


.globl hal_cpuInvalTLB
.type hal_cpuInvalTLB, %function
hal_cpuInvalTLB:
	mcr p15, 0, r0, c8, c7, 0
	bx lr
.size hal_cpuInvalTLB, .-hal_cpuInvalTLB


.globl hal_cpuInvalVA
.type hal_cpuInvalVA, %function
hal_cpuInvalVA:
	mcr p15, 0, r0, c8, c7, 1 /* ASID match */
	bx lr
.size hal_cpuInvalVA, .-hal_cpuInvalVA


.globl hal_cpuBranchInval
.type hal_cpuBranchInval, %function
hal_cpuBranchInval:
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 6
	bx lr
.size hal_cpuBranchInval, .-hal_cpuBranchInval


.globl hal_cpuICacheInval
.type hal_cpuICacheInval, %function
hal_cpuICacheInval:
	mov r0, #0
	mcr p15, 0, r0, c7, c5, 0
	bx lr
.size hal_cpuICacheInval, .-hal_cpuICacheInval


.globl hal_cpuGetUserTT
.type hal_cpuGetUserTT, %function
hal_cpuGetUserTT:
	mrc p15, 0, r0, c2, c0, 0
	bx lr
.size hal_cpuGetUserTT, .-hal_cpuGetUserTT


.globl hal_cpuSetUserTT
.type hal_cpuSetUserTT, %function
hal_cpuSetUserTT:
	mcr p15, 0, r0, c2, c0, 0
	bx lr
.size hal_cpuSetUserTT, .-hal_cpuSetUserTT


.globl hal_cpuSetContextId
.type hal_cpuSetContextId, %function
hal_cpuSetContextId:
	mcr p15, 0, r0, c13, c0, 1
	bx lr
.size hal_cpuSetContextId, .-hal_cpuSetContextId


.globl hal_cpuGetContextId
.type hal_cpuGetContextId, %function
hal_cpuGetContextId:
	mrc p15, 0, r0, c13, c0, 1
	bx lr
.size hal_cpuGetContextId, .-hal_cpuGetContextId


.globl _hal_cpuSetKernelStack
.type _hal_cpuSetKernelStack, %function
_hal_cpuSetKernelStack:
	mcr p15, 0, r0, c13, c0, 4
	dsb
	isb
	bx lr
.size _hal_cpuSetKernelStack, .-_hal_cpuSetKernelStack


.globl hal_cpuGetMIDR
.type hal_cpuGetMIDR, %function
hal_cpuGetMIDR:
	mrc p15, 0, r0, c0, c0, 0
	bx lr
.size hal_cpuGetMIDR, .-hal_cpuGetMIDR


.globl hal_cpuGetPFR0
.type hal_cpuGetPFR0, %function
hal_cpuGetPFR0:
	mrc p15, 0, r0, c0, c1, 0
	bx lr
.size hal_cpuGetPFR0, .-hal_cpuGetPFR0


.globl hal_cpuGetPFR1
.type hal_cpuGetPFR1, %function
hal_cpuGetPFR1:
	mrc p15, 0, r0, c0, c1, 1
	bx lr
.size hal_cpuGetPFR1, .-hal_cpuGetPFR1


.globl hal_longjmp
.type hal_longjmp, %function
hal_longjmp:
	cpsid if
	add sp, r0, #8
	b _hal_cpuRestoreCtx
.size hal_longjmp, .-hal_longjmp


.globl hal_jmp
.type hal_jmp, %function
hal_jmp:
	cpsid if
	mov r4, r0
	mov r5, r1
	mov r6, r2
	mov r7, r3
	cmp r6, #0
	bne 2f
	mov sp, r5
	subs r7, #1
	bmi 1f
	pop {r0}
	subs r7, #1
	bmi 1f
	pop {r1}
	subs r7, #1
	bmi 1f
	pop {r2}
	subs r7, #1
	bmi 1f
	pop {r3}
1:	cpsie if
	blx r4
2:	mov sp, r6
	subs r7, #1
	bmi 3f
	pop {r0}
	subs r7, #1
	bmi 3f
	pop {r1}
	subs r7, #1
	bmi 3f
	pop {r2}
	subs r7, #1
	bmi 3f
	pop {r3}
3:	cps #0x12
	mov r5, #0x10
	tst r4, #1
	orrne r5, r5, #(1 << 5)
	push {r5}
	push {r4}
	rfefd sp!
.size hal_jmp, .-hal_jmp
