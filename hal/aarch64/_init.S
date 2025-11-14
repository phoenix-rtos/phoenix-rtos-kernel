/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for AArch64 architecture
 *
 * Copyright 2024 Phoenix Systems
 * Author: Jacek Maksymowicz
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
.extern stacks_el1
.extern hal_syspage
.extern relOffs
.extern nCpusStarted
.extern _end


/* 16-bit ASID
 * Params for TTBR1:
 * * 4 KB granule
 * * Inner shareable
 * * Outer cacheable
 * * Inner cacheable
 * * Disable translation through TTBR1
 * * ASID determined by TTBR0
 * * 1 GB virtual address range
 * Params for TTBR0:
 * * 4 KB granule
 * * Inner shareable
 * * Outer cacheable
 * * Inner cacheable
 * * Enable translation through TTBR0
 * * 512 GB virtual address range */
#define TCR_EL1_VALUE ( \
	(1 << 36) | \
	(2 << 30) | \
	(3 << 28) | \
	(1 << 26) | \
	(1 << 24) | \
	(1 << 23) | \
	(0 << 22) | \
	(34 << 16) | \
	(0 << 14) | \
	(3 << 12) | \
	(1 << 10) | \
	(1 << 8) | \
	(0 << 7) | \
	(25 << 0) \
)

/* Index 0: Normal memory, inner and outer cacheable, write-back, non-transient read-allocate, write-allocate
 * Index 1: Normal memory, inner and outer non-cacheable
 * Index 2: Device memory, Gathering, re-ordering disabled, early write acknowledge enabled (equiv. to AArch32 "Device" memory)
 * Index 3: Device memory, Gathering, re-ordering, early write acknowledge all disabled (equiv. to AArch32 "Strongly ordered" memory)
*/
#define MAIR_EL1_VALUE 0x444FF

/* Privileged RWX, unprivileged no-access, normal cacheable, inner shareable, access flag, global */
#define DEFAULT_ATTRS 0x703

/* Needs to be updated if hal_syspage_t is changed */
#define SYSPAGE_SIZE_OFFSET 8
#define SYSPAGE_PKERNEL_OFFSET 16

#define PMAP_COMMON_KERNEL_TTL2 (pmap_common)
#define PMAP_COMMON_KERNEL_TTL3 (pmap_common + SIZE_PAGE)
#define PMAP_COMMON_DEVICES_TTL3 (pmap_common + (2 * SIZE_PAGE))
#define PMAP_COMMON_SCRATCH_TT (pmap_common + (3 * SIZE_PAGE))
#define PMAP_COMMON_SCRATCH_PAGE (pmap_common + (4 * SIZE_PAGE))
#define PMAP_COMMON_STACK (pmap_common + (5 * SIZE_PAGE))
#define VADDR_SYSPAGE (_hal_syspageCopied)

.section .init, "ax"
.global _start
.type _start, %function

.org 0
_start:
	/* Assumptions:
	 * x9 => PA of syspage from PLO */
	/* Mask all interrupts */
	msr daifSet, #0xf

	/* Interesting bits:
		EE == 0, E0E == 0 => little-endian at EL1 and EL0
		WXN == 0 => write permission doesn't imply execute never
		nTWE == 0, nTWI == 0 => trap WFE/WFI instructions at EL0
		UMA == 0 => trap accesses to DAIF system reg. from EL0
		SED == 1 => trap SETEND instruction in AArch32
		SA0 == 1, SA == 1 => alignment checks for SP accesses
		A == 0 => disable data alignment checks
		C, I, M == 0 => disable D-cache, I-cache, MMU (for now)
		UCT == 1 => don't trap accesses to CTR_EL0 (cache type reg.) from EL0
		DZE == 1 => don't trap DC ZVA instructions from EL0
		UCI == 0 => trap cache maintenance instructions executed at EL0
	 */
	ldr x0, =0x30c0c938
	msr sctlr_el1, x0

	/* Trap floating point access */
	mrs x0, cpacr_el1
	and x0, x0, #(~(0x3 << 20))
	msr cpacr_el1, x0
	isb

	/* Invalidate caches and TLB */
	ic iallu
	bl hal_cpuInvalDataCacheAll
	tlbi vmalle1
	dsb ish
	isb

	/* Enable performance monitors */
	mrs x0, pmcr_el0
	orr x0, x0, #0x7 /* reset cycle counter, reset event counter, enable */
	orr x0, x0, #0x40 /* activate 64-bit counting mode */
	msr pmcr_el0, x0
	mrs x0, pmcntenset_el0
	orr x0, x0, #(1 << 31)
	msr pmcntenset_el0, x0

#ifdef __TARGET_AARCH64A53
	/* Enable SMP */
	mrs x0, s3_1_c15_c2_1 /* CPUECTLR_EL1 */
	orr x0, x0, #(0x1 << 6)
	msr s3_1_c15_c2_1, x0

	/* L1 Data prefetch control - 5, Enable device split throttle, 2 independent data prefetch streams */
	/* Set ENDCCASCI bit in CPUACTLR_EL1 register - needed due to Cortex-A53 erratum #855873 */
	ldr	x0, =0x1000080ca000
	msr	s3_1_c15_c2_0, x0 /* CPUACTLR_EL1 */
#endif

	/* Set up MMU */
	ldr x0, =(TCR_EL1_VALUE)
	mrs x1, id_aa64mmfr0_el1
	and x1, x1, #0x7
	orr x0, x0, x1, lsl #32
	msr tcr_el1, x0

	ldr x0, =(MAIR_EL1_VALUE)
	msr mair_el1, x0

	/* Note: ADRP instruction will generate a PC-relative address.
	 * Because now PC is in physical memory, the resulting address will also be physical */
	/* Create temporary TTL1 that maps 2 GB around syspage->pkernel
	 * Note that there may be garbage data in this table - we will take care of it later. */
	adrp x0, PMAP_COMMON_SCRATCH_TT
	/* Map syspage->pkernel ~ (syspage->pkernel + 1 GB) VA => syspage->pkernel ~ (syspage->pkernel + 1 GB) PA */
	ldr x1, [x9, #(SYSPAGE_PKERNEL_OFFSET)]
	mov x3, #0xf01 /* Privileged RWX, unprivileged no-access, normal cacheable, inner shareable, access flag, not global */
	mov x4, #(1 << 30)
	lsr x2, x1, #30
	orr x1, x3, x2, lsl #30
	str x1, [x0, x2, lsl 3]
	add x1, x1, x4
	add x2, x2, #1
	str x1, [x0, x2, lsl 3]

	/* Map syspage VA => syspage PA */
	lsr x2, x9, #30
	orr x1, x3, x2, lsl #30
	str x1, [x0, x2, lsl 3]
	msr ttbr0_el1, x0

	adrp x0, PMAP_COMMON_KERNEL_TTL2
	msr ttbr1_el1, x0

	/* Turn on MMU and caches */
	dsb ish
	mrs x0, sctlr_el1
	orr x0, x0, #(1 << 12)
	orr x0, x0, #(1 << 2)
	orr x0, x0, #(1 << 0)
	msr sctlr_el1, x0
	dsb ish
	isb

	/* Translation is now taking place through PMAP_COMMON_SCRATCH_TT
	 * Before we can jump to kernel in virtual memory we must set up PMAP_COMMON_KERNEL_TTL2 table */

	/* Zero out an additional page that will be used a bit later */
	adrp x0, PMAP_COMMON_SCRATCH_PAGE
	bl _fill_page_zero

	/* Initialize counter of started CPUs.
	 * The counter starts out uninitialized. We want to initialize it only once by only one core -
	 * but cores can start in any order and the variable must be initialized before any core starts waiting.
	 * To solve this, we use a flag in the code section. Right now the code section is writable,
	 * but it will be changed to read-only later. */
	adrp x1, nCpusStarted
	adr x2, _initNCpusFlag
.LretryWrite:
	ldaxr w0, [x2]
	cbz w0, .LnCpusAlreadyInited
	stlxr w3, wzr, [x2]
	cbnz w3, .LretryWrite
	str wzr, [x1, #:lo12:nCpusStarted]
.LnCpusAlreadyInited:
	clrex

	/* Trap all cores except core 0 */
	mrs x8, mpidr_el1
	and x8, x8, #0xff
	cbnz x8, _other_core_trap

	/* Set up translation tables */
	adrp x0, PMAP_COMMON_KERNEL_TTL2
	bl _fill_page_zero

	/* Map TTL2 VADDR_KERNEL ~ (VADDR_KERNEL + 2 MB) => PMAP_COMMON_KERNEL_TTL3 */
	adrp x1, PMAP_COMMON_KERNEL_TTL3
	orr x1, x1, #0x3 /* Valid, table descriptor */
	str x1, [x0]

	/* Map TTL2 (VADDR_MAX - 2 MB) ~ VADDR_MAX => PMAP_COMMON_DEVICES_TTL3 */
	add x2, x0, #(SIZE_PAGE - 8)
	adrp x0, PMAP_COMMON_DEVICES_TTL3
	orr x1, x0, #0x3 /* Valid, table descriptor */
	str x1, [x2]

	/* Map TTL3 (VADDR_MAX - 2 MB) ~ VADDR_MAX => no access */
	bl _fill_page_zero

	/* Map TTL3 VADDR_KERNEL ~ (VADDR_KERNEL + 2 MB) => syspage->pkernel ~ (syspage->pkernel + 2 MB) PA */
	adrp x0, PMAP_COMMON_KERNEL_TTL3
	ldr x1, [x9, #(SYSPAGE_PKERNEL_OFFSET)]
	mov x2, #(DEFAULT_ATTRS)
	orr x1, x1, x2
	bl _fill_page_descr

	/* Ensure changes to translation tables are visible */
	dsb ish
	isb

	/* Activate translation through ttbr1_el1 */
	mrs x0, tcr_el1
	bic x0, x0, #(1 << 23)
	msr tcr_el1, x0
	isb

	ldr x0, =_core_0_virtual
	br x0

_core_0_virtual:
	/* Copy syspage to the designated spot in virtual memory */
	adrp x1, VADDR_SYSPAGE
	add x1, x1, :lo12:VADDR_SYSPAGE
	sub x2, x1, x9
	adrp x0, relOffs
	str x2, [x0, #:lo12:relOffs]
	adrp x0, hal_syspage
	str x1, [x0, #:lo12:hal_syspage]
	ldr x0, [x9, #(SYSPAGE_SIZE_OFFSET)]
	add x0, x0, x9
1:
	ldr x3, [x9], #8
	str x3, [x1], #8
	cmp x9, x0
	b.lo 1b

	bl _set_up_vbar_and_stacks
	b main


_other_core_trap:
	dsb ish
	wfe
	ldr x0, [x1, #:lo12:nCpusStarted]
	cbz x0, _other_core_trap

	/* Ensure changes to translation tables are visible */
	dsb ish
	isb

	/* Activate translation through ttbr1_el1 */
	mrs x0, tcr_el1
	bic x0, x0, #(1 << 23)
	msr tcr_el1, x0
	isb

	ldr x0, =_other_core_virtual
	br x0

_other_core_virtual:
	bl _set_up_vbar_and_stacks
	bl _hal_interruptsInitPerCPU
	bl _hal_cpuInit

	msr daifClr, #7
1:
	wfi
	b 1b


_fill_page_zero:
	/* Fill page with zeroes
	 * x0 => base address
	 * clobbers x1 */
	add x1, x0, #(SIZE_PAGE)
1:
	stp xzr, xzr, [x1, #-0x10]!
	stp xzr, xzr, [x1, #-0x10]!
	cmp x0, x1
	b.lo 1b
	ret

_fill_page_descr:
	/* Fill page with descriptor entries
	 * x0 => base address
	 * x1 => target value
	 * clobbers x0, x1, x2, x3 */
	add x2, x1, #(SIZE_PAGE)
	add x3, x0, #(SIZE_PAGE)
1:
	stp x1, x2, [x0], #0x10
	add x1, x1, #(2 * SIZE_PAGE)
	add x2, x2, #(2 * SIZE_PAGE)
	stp x1, x2, [x0], #0x10
	add x1, x1, #(2 * SIZE_PAGE)
	add x2, x2, #(2 * SIZE_PAGE)
	cmp x0, x3
	b.lo 1b
	ret

_set_up_vbar_and_stacks:
	/* Assumptions:
	 * Running in kernel virtual memory (above VADDR_KERNEL)
	 * x8 => CPU ID
	 * clobbers x0, x1 */
	/* Set vector table */
	adrp x0, _vector_table
	add x0, x0, :lo12:_vector_table
	msr vbar_el1, x0

	/* Set TTBR0 translation table to the page that was cleared out earlier,
	 * then invalidate all TLB - this will wipe any trash entries that may have been
	 * loaded into TLB and ensure no more are loaded */
	adrp x0, PMAP_COMMON_SCRATCH_PAGE
	msr ttbr0_el1, x0
	dsb ish
	tlbi vmalle1
	dsb ish

	/* Set up kernel stack per core */
	msr spsel, #1
	adrp x0, PMAP_COMMON_STACK
	mov x1, SIZE_INITIAL_KSTACK
	madd x0, x1, x8, x0
	add sp, x0, x1
	ret

.size _start, .-_start
.ltorg

_initNCpusFlag:
	.word 1


.align 4
.globl hal_cpuInvalDataCacheAll
.type hal_cpuInvalDataCacheAll, %function
hal_cpuInvalDataCacheAll:
.cfi_startproc
	/* 5.3.1 Cleaning and invalidating the caches */
	mov x0, #0x0             /* x0 = Cache level */
	msr csselr_el1, x0       /* 0x0 for L1 Dcache 0x2 for L2 Dcache. */
	mrs x4, ccsidr_el1       /* Read Cache Size ID. */
	and x1, x4, #0x7
	add x1, x1, #0x4         /* x1 = Cache Line Size. */
	mov x3, #0x7fff
	and x2, x3, x4, lsr #13  /* x2 = Cache Set Number – 1. */
	mov x3, #0x3ff
	and x3, x3, x4, lsr #3   /* x3 = Cache Associativity Number – 1. */
	clz w4, w3               /* x4 = way position in the CISW instruction. */
	mov x5, #0               /* x5 = way counter way_loop. */
.Lway_loop:
	mov x6, #0               /* x6 = set counter set_loop. */
.Lset_loop:
	lsl x7, x5, x4
	orr x7, x0, x7           /* Set way. */
	lsl x8, x6, x1
	orr x7, x7, x8           /* Set set. */
	dc isw, x7               /* Invalidate cache line. */
	add x6, x6, #1           /* Increment set counter. */
	cmp x6, x2               /* Last set reached yet? */
	b.le .Lset_loop          /* If not, iterate set_loop, */
	add x5, x5, #1           /* else, next way. */
	cmp x5, x3               /* Last way reached yet? */
	b.le .Lway_loop          /* If not, iterate way_loop. */
	ret
.cfi_endproc
.size hal_cpuInvalDataCacheAll, .-hal_cpuInvalDataCacheAll
.ltorg


#include "hal/aarch64/_exceptions.S"

.section ".bss"
.align 4
_hal_syspageCopied:
	.zero SIZE_PAGE
.size _hal_syspageCopied, . - _hal_syspageCopied
