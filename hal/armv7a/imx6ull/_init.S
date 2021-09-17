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

#include "cpu.h"
#include "pmap.h"
#include "config.h"

.extern pmap_common
.extern syspage
.extern relOffs
.extern _end
.extern imx6ull_bootReason


#define ADDR_STACK    0x803ff000
#define ADDR_TTL1     (pmap_common - VADDR_KERNEL + ADDR_DDR)
#define ADDR_TTL2_K   (ADDR_TTL1 + 4 * SIZE_PAGE)
#define ADDR_TTL2_EXC (ADDR_TTL2_K + SIZE_PAGE)

#define VADDR_SYSPAGE (_end + SIZE_PAGE - 1)
#define VADDR_UART1   (VADDR_SYSPAGE + 2 * SIZE_PAGE)
#define VADDR_UART2   (VADDR_UART1 + SIZE_PAGE)
#define VADDR_GIC     (VADDR_UART2 + SIZE_PAGE)
#define VADDR_GPT     (VADDR_GIC + 4 * SIZE_PAGE)
#define VADDR_CCM     (VADDR_GPT + SIZE_PAGE)

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

	ldr r0, =relOffs
	ldr r1, =#VADDR_SYSPAGE
	lsr r1, #12
	lsl r1, #12

	sub r0, r0, #VADDR_KERNEL
	add r0, r0, #ADDR_DDR
	sub r2, r1, r9
	str r2, [r0]

	ldr r0, [r9, #4] /* syspage size address - syspage address + sizeof(hal_syspage_t) */
	add r2, r9, r0   /* end of the syspage */

	ldr r0, =syspage
	sub r0, r0, #VADDR_KERNEL
	add r0, r0, #ADDR_DDR
	str r1, [r0]

	sub r1, r1, #VADDR_KERNEL
	add r1, r1, #ADDR_DDR

syspage_cpy:
	ldr r3, [r9], #4
	str r3, [r1], #4
	cmp r9, r2
	blo syspage_cpy

	/* Save and clear boot reason */
	ldr r0, =0x20d8008
	ldr r7, [r0]
	movw r2, #0xffff
	str r2, [r0]

	/* WARNING: R7 - can't touch this until MMU enable */

	/* Clear PERSIST_SECONDARY_BOOT bit */
	ldr r0, =0x020d8044
	ldr r1, [r0]
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
#include "../_hal.S"
