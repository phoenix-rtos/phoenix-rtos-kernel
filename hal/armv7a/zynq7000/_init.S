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

#include "config.h"
#include <arch/cpu.h>
#include <arch/pmap.h>

.extern pmap_common
.extern syspage
.extern relOffs
.extern _end

#define PA_KERNEL (ADDR_DDR)
#define PA_OF(va) ((va) - VADDR_KERNEL + PA_KERNEL)
#define TTL1_OFFSET_OF(va) ((va >> 20) << 2)

#define VA_TTL1     (pmap_common) /* u32 kpdir[0x1000] */
#define VA_TTL2_K   (VA_TTL1 + 4 * SIZE_PAGE) /* u32 kptab[0x400] */
#define VA_TTL2_EXC (VA_TTL2_K + SIZE_PAGE) /* u32 excptab[0x400] */
#define VA_TTL_END  (VA_TTL2_EXC + SIZE_PAGE)
#define PA_STACK    (ADDR_DDR + 4 * 1024 * 1024 - SIZE_PAGE)

#define VA_SYSPAGE (_end + SIZE_PAGE - 1)
#define VA_UART0   (VA_SYSPAGE + 2 * SIZE_PAGE)
#define VA_UART1   (VA_UART0 + SIZE_PAGE)
#define VA_GIC     (VA_UART1 + SIZE_PAGE)
#define VA_TTC     (VA_GIC + 4 * SIZE_PAGE)

#define PA_UART0 0xe0000000
#define PA_UART1 0xe0001000
#define PA_SLCR  0xf8000000
#define PA_TTC   0xf8001000

/* Attributes for kernel pages:
 * Small page, XN = 0 (allow Execute), TEX = 1 C = 1 B = 1 (Outer and Inner Write-Back, Write-Allocate),
 * AP = 0x1 (R/W from PL1 only), S = 1 (Shareable), nG = 0 (Global) */
#define KERNEL_PAGE_ATTR 0x45E

/* Attributes for device pages:
 * Small page, XN = 1 (Execute Never), TEX = 0 C = 0 B = 1 (Shared Device),
 * AP = 0x1 (R/W from PL1 only), S = 0 (unused), nG = 0 (Global) */
#define DEVICE_PAGE_ATTR 0x17

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

/*
	Fill 4 words in memory starting from r0 with value r1 incremented by r2 after every step
	r0 - pointer to destination, set past-the-end on return
	r1 - initial value, set past-the-end on return
	r2 - increment step, constant
	r3 - will be set to 0
 */
_cpy4:
	mov r3, #4
	str r1, [r0], #4
	add r1, r1, r2
	subs r3, #1
	bne _cpy4 + 4
	mov pc, lr


/* startup code */
_start:
	/* r9 now contains PA of syspage from PLO */
	cpsid aif, #SYS_MODE

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

	/* Read CPU ID, only core 0 continues initialization */
	mrc p15, 0, r1, c0, c0, 5       /* Read Multiprocessor Affinity Register */
	ands r1, r1, #0xf               /* Extract CPU ID                        */
	beq structs_init
wait_loop:
	wfi
	b wait_loop

structs_init:
	/* init memory structures (relOffs, syspage, TTLs) */
	ldr r1, =#VA_SYSPAGE
	lsr r1, #12
	lsl r1, #12
	sub r2, r1, r9

	ldr r0, =#PA_OF(relOffs)
	str r2, [r0]

	ldr r0, [r9, #4] /* load syspage size from syspage address + sizeof(hal_syspage_t) */
	add r2, r9, r0   /* set r2 to end of the syspage */

	ldr r0, =#PA_OF(syspage)
	str r1, [r0]

	sub r1, r1, #VADDR_KERNEL
	add r1, r1, #ADDR_DDR
	mov r0, r9

syspage_cpy:
	ldr r3, [r0], #4
	str r3, [r1], #4
	cmp r0, r2
	blo syspage_cpy

	/* Initialize MMU translation tables */
	/* Clear everything to 0 (invalid entry) */
	ldr r5, =PA_OF(VA_TTL1)
	mov r1, #0
	mov r2, #(VA_TTL_END - VA_TTL1)
clear_ttls:
	subs r2, #4
	str r1, [r5, r2]
	bne clear_ttls

	/* Needed to execute first stage of kernel, should be unmapped later */
	/* Map 4 MB V 0x00100000 -> P 0x00100000 */
	add r0, r5, #TTL1_OFFSET_OF(PA_KERNEL)                  /* Entry address: TTL1 base address + entry index * 4 B (entry size)           */
	ldr r1, =((PA_KERNEL & ~0xfffff) | (0x1 << 10) | 0x2)  /* Section entry: base address - DDR, AP = 01, APX = 0 (privileged access only) */
	mov r2, #0x100000                                      /* Size of section: 1 MB                                                       */
	bl _cpy4                                               /* Fill 4 entries in TTL1                                                      */


	/* Kernel TTL1 entries
	 * map 4 MB V VADDR_KERNEL -> TTL2 in pmap_common.kptab */
	add r0, r5, #TTL1_OFFSET_OF(VADDR_KERNEL)              /* Entry address: virtual kernel address + entry index * 4 B (entry size)  */
	ldr r1, =(PA_OF(VA_TTL2_K) + 1)                        /* Ptr to kernel's TTL2 (pmap_common.kptab); bits [1:0] = 1 defines TTL2   */
	mov r2, #0x400                                         /* Size of TTL2                                                            */
	bl _cpy4                                               /* Fill TTL1 with 4 TTL2's addresses; pmap_common.kptab consists of 4 TTL2 */


	/* Exception vectors and stack TTL1 entry
	 * map 4MB V 0xffc00000 -> TTL2 in pmap_common.excptab */
	ldr r0, =(PA_OF(VA_TTL1) + TTL1_OFFSET_OF(0xffc00000)) /* Entry address: TTL1 address + index (0xffc - 4 entries in TTL1) * 4 B*/
	ldr r1, =(PA_OF(VA_TTL2_EXC) + 1)                      /* Ptr to exceptions' TTL2 (pmap_common.excptab); bits [1:0] = 1 defines TTL2 */
	bl _cpy4                                               /* Fill TTL1 with 4 TTL2's addresses; pmap_common.excptab consists of 4 TTL2  */

	ldr r8, =(ADDR_DDR)

	/* Exceptions vectors TTL2 entry */
	/* Map V 0xffff0000 -> P 0x00100000 */
	ldr r0, =(PA_OF(VA_TTL2_EXC) + (0x3f0 << 2))          /* Entry address: 4 entries from the end in last TTL2 in pmap_common.excptab    */
	orr r1, r8, #0x1a                                     /* Ptr to physical address. Attributes: XN = 0, B = 0, C = 0, AP = 0x1, TEX = 0 */
	str r1, [r0]                                          /* Fill TTL2 entry                                                              */


	/* Stack TTL2 entry */
	/* Map V 0xfffff000 -> P PA_STACK  */
	ldr r0, =(PA_OF(VA_TTL2_EXC) + (0x3ff << 2))        /* Entry address: the last entry in 4 TTL2 in pmap_common.excptab               */
	ldr r1, =((PA_STACK & ~0xfff) | KERNEL_PAGE_ATTR)
	str r1, [r0]

	/* Set vector table pointer to virtual address */
	ldr r0, =_vector_table
	mcr p15, 0, r0, c12, c0, 0                            /* Write to VBAR (Vector Base Address Register) */


	/* Kernel TTL2 entries (pmap_common.kptab) */
	ldr r0, =PA_OF(VA_TTL2_K)
	ldr r1, =((PA_KERNEL & ~0xfff) + (1024 * SIZE_PAGE) | KERNEL_PAGE_ATTR) /* Ptr past-the-end of physical addresses */
	mov r2, #(4 * 1024)                                 /* size of pmap_common.kptab, it contains 4 TTL2 */
	/* Map the whole kernel memory */
kernel_ttl2:
	subs r2, r2, #4
	sub r1, #SIZE_PAGE
	str r1, [r0, r2]
	bne kernel_ttl2


	/* Change memory attributes of kernel page directory within TTL2 */
	ldr r1, =(pmap_common - VADDR_KERNEL)    /* offset of pmap_common.kpdir                             */
	add r0, r1, lsr #10                      /* r0 = PA_OF(VA_TTL2_K) + (offset of: pmap_common.kpdir >> 10) */
	add r1, r1, #PA_KERNEL                   /* physical address of pmap_common.kpdir                   */
	ldr r3, =#(KERNEL_PAGE_ATTR | 0x01)      /* Attributes: default + XN bit     */
	orr r1, r1, r3
	mov r2, #0x1000
	bl _cpy4
	bl _cpy4
	bl _cpy4
	bl _cpy4

	/* Also change attributes of kernel page tables */
	bl _cpy4

	/* Map perpehrals addresses */
	/* Map UART0 4 KB P 0xE0000000 -> V CEIL(_end, SIZE_PAGE) */
	ldr r0, =(VA_UART0 - VADDR_KERNEL)
	lsr r0, #12
	lsl r0, #2
	ldr r1, =PA_OF(VA_TTL2_K)
	add r0, r0, r1
	ldr r1, =(PA_UART0 | DEVICE_PAGE_ATTR)
	str r1, [r0], #4

	/* Map UART1 4KB P 0xE0001000 -> V CEIL(_end + SIZE_PAGE, SIZE_PAGE) */
	ldr r1, =(PA_UART1 | DEVICE_PAGE_ATTR)
	str r1, [r0], #4

	/* Map GIC 16 KB after UARTs */
	mrc p15, 4, r1, c15, c0, 0           /* Get GIC paddr */
	lsr r1, #16
	lsl r1, #16
	orr r1, r1, #DEVICE_PAGE_ATTR
	mov r2, #(1 << 12)
	bl _cpy4

	/* Map SLCR after GIC */
	ldr r1, =(PA_SLCR | DEVICE_PAGE_ATTR)
	str r1, [r0], #4

	/* Map TTC after SLCR */
	ldr r1, =(PA_TTC | DEVICE_PAGE_ATTR)
	str r1, [r0], #4


	/* Initialize MMU */
	mov r1, #1
	mcr p15, 0, r1, c2, c0, 2                  /* Write Translation Table Base Control Register */
	ldr r1, =PA_OF(VA_TTL1)
	orr r1, r1, #(1 | (1 << 6) | (3 << 3) | 2) /* Inner and outer cacheability */
	mcr p15, 0, r1, c2, c0, 0                  /* Write Translation Table Base Register 0       */
	mcr p15, 0, r1, c2, c0, 1                  /* Write Translation Table Base Register 1       */

	/* Set all Domains to Client */
	ldr r1, =0x55555555
	mcr p15, 0, r1, c3, c0, 0        /* Write Domain Access Control Register */

	mov r1, #0
	mcr p15, 0, r1, c13, c0, 1 		 /* Set ASID == 0 in CONTEXTIDR */

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

	/* Reset fault status/address registers to a known value */
	mov r0, #0xFFFFFFFF
	mcr p15, 0, r0, c5, c0, 0 /* DFSR */
	mcr p15, 0, r0, c6, c0, 0 /* DFAR */
	mcr p15, 0, r0, c5, c0, 1 /* IFSR */
	mcr p15, 0, r0, c6, c0, 2 /* IFAR */

	/* Setup stacks */
	eor r0, r0          /* initialize SP to top of memory */

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

	/* Jump to main() in virtual memory */
	ldr pc, =main

#include "hal/armv7a/_interrupts.S"
#include "hal/armv7a/_armv7a.S"
