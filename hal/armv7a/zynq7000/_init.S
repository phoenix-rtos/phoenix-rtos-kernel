/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for Cortex-A9 (ARMv7) architecture
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "pmap.h"
#include "config.h"
#include <arch/cpu.h>

.extern pmap_common
.extern syspage
.extern relOffs
.extern _end

#define ADDR_TTL1     (pmap_common - VADDR_KERNEL + ADDR_DDR)
#define ADDR_TTL2_K   (ADDR_TTL1 + 4 * SIZE_PAGE)
#define ADDR_TTL2_EXC (ADDR_TTL2_K + SIZE_PAGE)
#define ADDR_STACK    0x004ff000

#define VADDR_SYSPAGE (_end + SIZE_PAGE - 1)
#define VADDR_UART0   (VADDR_SYSPAGE + 2 * SIZE_PAGE)
#define VADDR_UART1   (VADDR_UART0 + SIZE_PAGE)
#define VADDR_GIC     (VADDR_UART1 + SIZE_PAGE)
#define VADDR_TTC     (VADDR_GIC + 4 * SIZE_PAGE)

#define PADDR_UART0 0xe0000000
#define PADDR_UART1 0xe0001000
#define PADDR_SLCR  0xf8000000
#define PADDR_TTC   0xf8001000

.arm

.section .init, "ax"
.global _start
.type _start, %function


.org 0
_vector_table:
	b _start
	b _exception_undef
	b _syscalls_dispatch
	b _exception_prefetch
	b _exception_abort
	.word 0
	b _interrupts_dispatch
	b _interrupts_dispatch

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

	/* Only CPU0 performs initialization, others go into WFI */
	mrc p15, 0, r1, c0, c0, 5       /* Read Multiprocessor Affinity Register */
	and r1, r1, #0xf                /* Extract CPU ID                        */
	cmp r1, #0
	beq initialize

wait_loop:
/* TODO: make appropriate action when other core than CPU0 is running */
	wfi
	b wait_loop


initialize:
	/* Enable PMU */
	mrc p15, 0, r0, c9, c12, 0       /* Read PMCR (Performance Monitor Control Register)  */
	orr r0, #0x7                     /* Cycle counter reset - bit[2], Performance counter reset - bit[1], enable all counters - bit[0] */
	mcr p15, 0, r0, c9, c12, 0       /* Write PMCR (Performance Monitor Control Register) */
	mrc p15, 0, r0, c9, c12, 1       /* Read CESR (Count Enable Set Register)             */
	orr r0, #1 << 31                 /* Enable cycle counter                              */
	mcr p15, 0, r0, c9, c12, 1       /* Write CESR (Count Enable Set Register)            */

	/* Enable SMP */
	mrc p15, 0, r1, c1, c0, 1
	orr r1, r1, #(1 << 6)
	mcr p15, 0, r1, c1, c0, 1

	/* Disable MMU */
	mrc p15, 0, r1, c1, c0, 0        /* Read SCTLR (System Control Register) data  */
	bic r1, r1, #0x1                 /* clear first bit: disable MMU               */
	mcr p15, 0, r1, c1, c0, 0        /* Write SCTLR (System Control Register) data */


	/* Disable L1 caches */
	mrc p15, 0, r1, c1, c0, 0        /* Read SCTLR (System Control Register) data  */
	bic r1, r1, #(0x1 << 12)         /* Disable ICache                             */
	bic r1, r1, #(0x1 << 2)          /* Disable DCache                             */
	mcr p15, 0, r1, c1, c0, 0        /* Write SCTLR (System Control Register) data */

	/* Invalidate L1 ICache */
	mov r1, #0
	mcr p15, 0, r1, c7, c5, 0        /* Clear ICIALLU */

	/* Invalidate L1 DCache. Based on ARM Cortex-A Series Programmer's Guide */
	mrc p15, 1, r0, c0, c0, 0        /* Read CCSIDR (Cache Size Identification Register) */
	mov r3, #0x1ff
	and r0, r3, r0, lsr #13          /* r0 = number of sets -                            */
	mov r1, #0                       /* r1 = way counter way_loop                        */
way_loop:
	mov r3, #0                       /* r3 = set counter set_loop                        */
set_loop:
	mov r2, r1, lsl #30
	orr r2, r3, lsl #5               /* r2 = set/way cache operation format              */
	mcr p15, 0, r2, c7, c6, 2        /* Invalidate line described by r2; write to DCISW  */
	add r3, r3, #1                   /* Increment set counter                            */
	cmp r0, r3                       /* Check whether last set was reached               */
	bgt set_loop                     /* Iterate set_loop                                 */
	add r1, r1, #1                   /* Increment way counter                            */
	cmp r1, #4                       /* Check whether last way was reached               */
	bne way_loop


	/* Invalidate TLB */
	mcr p15, 0, r1, c8, c7, 0

	/* Init TTL1 */
	ldr r5, =ADDR_TTL1
	mov r1, #0
	mov r2, #(4096 * 6 - 4)           /* Size of kpdir, kptab and excptab in pmap_common */
clear_ttl1:
	str r1, [r5, r2]
	subs r2, #4
	bne clear_ttl1
	str r1, [r5]

	/* In order to execute first stage of kernel */
	/* Map 4 MB P 0x00100000 -> V 0x00100000 */
	add r0, r5, #((ADDR_DDR >> 20) << 2)                  /* Entry address: TTL1 base address + entry index * 4 B (entry size)           */
	ldr r1, =((ADDR_DDR & ~0xfffff) | (0x1 << 10) | 0x2)  /* Section entry: base addres - DDR, AP = 01, APX = 0 (privileged access only) */
	mov r2, #0x100000                                     /* Size of section: 1 MB                                                       */
	bl _cpy4                                              /* Fill 4 entries in TTL1                                                      */


	/* Kernel TTL1 entries
	 * map 4 MB P 0x00100000 -> V 0xc0000000 */
	add r0, r5, #((VADDR_KERNEL >> 20) << 2)              /* Entry address: virtual kernel address + entry index * 4 B (entry size)  */
	ldr r1, =(ADDR_TTL2_K + 1)                            /* Ptr to kernel's TTL2 (pmap_common.kptab); bits [1:0] = 1 defines TTL2   */
	mov r2, #0x400                                        /* Size of TTL2                                                            */
	bl _cpy4                                              /* Fill TTL1 with 4 TTL2's addresses; pmap_common.kptab consists of 4 TTL2 */


	/* Exceptions vectors and stack TTL1 entry
	 * map 4MB V 0xffc00000 -> TTL2 of pmap_common.excptab */
	ldr r0, =(ADDR_TTL1 + (0xffc << 2))                   /* Entry address: TTL1 address + index (0xffc - 4 entries in TTL1) * 4 B*/
	ldr r1, =(ADDR_TTL2_EXC + 1)                          /* Ptr to exception's TTL2 (pmap_common.excptab); bits [1:0] = 1 defines TTL2 */
	bl _cpy4                                              /* Fill TTL1 with 4 TTL2's addresses; pmap_common.excptab consists of 4 TTL2  */

	ldr r8, =(ADDR_DDR)

	/* Exceptions vectors TTL2 entry */
	/* Map P 0x00100000 -> V 0xffff0000 */
	ldr r0, =(ADDR_TTL2_EXC + (0x3f0 << 2))               /* Entry address: 4 entries from the end in last TTL2 in pmap_common.excptab    */
	orr r1, r8, #0x1a                                     /* Ptr to physical address. Attributes: XN = 0, B = 0, C = 0, AP = 0x3, TEX = 0 */
	str r1, [r0]                                          /* Fill TTL2 entry                                                              */

	/* Stack TTL2 entry */
	/* Map P ADDR_STACK -> V 0xfffff000 */
	ldr r0, =(ADDR_TTL2_EXC + (0x3ff << 2))               /* Entry address: the last entry in 4 TTL2 in pmap_common.excptab               */
	ldr r1, =((ADDR_STACK & ~0xfff) | 0x1e)               /* Ptr to physical address. Attributes: XN = 0, B = 1, C = 1, AP = 0x3, TEX = 0 */
	str r1, [r0]                                          /* Fill TTL2 entry                                                              */


	/* Set vector table pointer to virtual address */
	ldr r0, =_vector_table
	mcr p15, 0, r0, c12, c0, 0                            /* Write to VBAR (Vector Base Address Register) */


	/* Kernel TTL2 entries (pmap_common.kptab) */
	ldr r0, =ADDR_TTL2_K
	ldr r1, =((ADDR_DDR & ~0xfff) + (1024 * SIZE_PAGE) | 0x1e)    /* Ptr to physical address. Attributes: XN = 0, B = 1, C = 1, AP = 0x3, TEX = 0 */
	mov r2, #(4 * 1024)                                           /* size of pmap_common.kptab, it contains 4 TTL2 */
	/* Map the whole kernel memory */
kernel_ttl2:
	subs r2, r2, #4
	sub r1, #SIZE_PAGE
	str r1, [r0, r2]
	bne kernel_ttl2


	/* Kernel page directory: change attributes of pmap_common structure */
	ldr r1, =(pmap_common - VADDR_KERNEL)    /* offset of pmap_common.kpdir                             */
	add r0, r1, lsr #10                      /* r0 = ADDR_TTL2_K + (offset of: pmap_common.kpdir >> 10) */
	add r1, r1, #ADDR_DDR                    /* physical address of pmap_common.kpdir                   */
	orr r1, r1, #0x1f                        /* Attributes: XN = 1, B = 1, C = 1, AP = 0x3, TEX = 0     */
	mov r2, #0x1000
	bl _cpy4
	bl _cpy4
	bl _cpy4
	bl _cpy4

	/* Kernel page tables */
	bl _cpy4

	/* Map perpehrals addresses */
	/* Map UART0 4 KB P 0xE0000000 -> V CEIL(_end, SIZE_PAGE) */
	ldr r0, =(VADDR_UART0 - VADDR_KERNEL)
	lsr r0, #12
	lsl r0, #2
	ldr r1, =ADDR_TTL2_K
	add r0, r0, r1
	ldr r1, =(PADDR_UART0 | 0x12)
	str r1, [r0], #4

	/* Map UART1 4KB P 0xE0001000 -> V CEIL(_end + SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =(PADDR_UART1 | 0x12)
	str r1, [r0], #4

	/* Map GIC 16 KB after UARTs */
	mrc p15, 4, r1, c15, c0, 0           /* Get GIC paddr */
	lsr r1, #16
	lsl r1, #16
	orr r1, r1, #0x12
	mov r2, #(1 << 12)
	bl _cpy4

	/* Map SLCR after GIC */
	ldr r1, =(PADDR_SLCR | 0x12)
	str r1, [r0], #4

	/* Map TTC after SLCR */
	ldr r1, =(PADDR_TTC | 0x12)
	str r1, [r0], #4


	/* Initialize MMU */
	mov r1, #1
	mcr p15, 0, r1, c2, c0, 2        /* Write Translation Table Base Control Register */
	ldr r1, =ADDR_TTL1
	orr r1, r1, #(1 | (1 << 6))      /* Inner cacheability                            */
	orr r1, r1, #(3 << 3)            /* Outer cacheability                            */
	mcr p15, 0, r1, c2, c0, 0        /* Write Translation Table Base Register 0       */
	mcr p15, 0, r1, c2, c0, 1        /* Write Translation Table Base Register 1       */

	/* Set all Domains to Client */
	ldr r1, =0x55555555
	mcr p15, 0, r1, c3, c0, 0        /* Write Domain Access Control Register */

	/* Enable L1 Caches */
	mrc p15, 0, r1, c1, c0, 0        /* Read SCTLR (System Control Register) data  */
	orr r1, r1, #(0x1 << 2)          /* Enable data cache                          */
	orr r1, r1, #(0x1 << 12)         /* Enable instruction cache                   */
	orr r1, r1, #(0x1 << 11)         /* Enable branch prediction                   */
	bic r1, r1, #(0x1 << 28)         /* Disable TEX remap                          */
	mcr p15, 0, r1, c1, c0, 0        /* Write SCTLR (System Control Register) data */


	/* Enable MMU */
	mrc p15, 0, r1, c1, c0, 0        /* Read Control Register configuration data  */
	orr r1, r1, #1                   /* Enable MMU settinh bit 0                  */
	mcr p15, 0, r1, c1, c0, 0        /* Write Control Register configuration data */
	dsb
	isb


	/* Setup stacks */
	eor r0, r0        /* initial SP */

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
	mrc p15, 0, r0, c1, c0, 2                 /* Read CPACR into R0                                 */
	orr r0, r0, #((0x3 << 22) | (0x3 << 20))  /* Set CP11 and CP10: Privileged and User mode access */
	mcr p15, 0, r0, c1, c0, 2                 /* Write R0 to CPACR                                  */
	vmrs r0, fpexc
	orr r0, r0, #(0x1 << 30)                  /* FPU enable bit                                     */
	vmsr fpexc, r0

	ldr pc, =main

#include "../_interrupts.S"
#include "../_armv7a.S"
