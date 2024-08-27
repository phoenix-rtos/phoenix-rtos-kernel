/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low level initialization
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "config.h"
#include <arch/cpu.h>
#include <arch/pmap.h>

#include "hal/sparcv8leon/srmmu.h"

#define ADDR_SRAM 0x40000000

#define PHY_ADDR(va) ((va) - VADDR_KERNEL + ADDR_SRAM)

#define ADDR_CTXTAB    PHY_ADDR(pmap_common)
#define ADDR_PDIR1     (ADDR_CTXTAB + 0x400)
#define ADDR_PDIR2     (ADDR_PDIR1 + 0x400)
#define ADDR_PDIR3     (ADDR_CTXTAB + 0x1000)

#define VADDR_SYSPAGE     _hal_syspageCopied
#define PADDR_SYSPAGE     PHY_ADDR(VADDR_SYSPAGE)

#define VADDR_STACK (pmap_common + 7 * SIZE_PAGE)


.extern syspage
.extern _end


.macro calc_ptd paddr out
	srl \paddr, 6, \out
	sll \out, 2, \out
	or \out, PAGE_DESCR, \out
.endm


.section ".text"
.align   4
.global  _init
_init:
	wr %g0, %wim
	nop
	nop
	nop

	wr %g0, PSR_S, %psr

	/* Get CPU ID */
	rd %asr17, %g1
	srl %g1, 28, %g1
	cmp %g1, %g0
	bnz _init_core
	nop

	/* %g2 = syspage pa (from plo) */
	set PHY_ADDR(syspage), %g3
	set VADDR_SYSPAGE, %g1

	/* store VADDR_SYSPAGE in syspage */
	st %g1, [%g3]

	set PHY_ADDR(relOffs), %g4
	/* %l0 = offset between VADDR_SYSPAGE and syspage pa */
	sub %g1, %g2, %l0
	st %l0, [%g4]

	set PADDR_SYSPAGE, %g3
	/* calculate pa of syspage end */
	ld [%g2 + 4], %l1 /* %l1 = syspage->size */
	add %g2, %l1, %l2 /* %l2 = plo syspage end */

syspage_cpy:
	ld [%g2], %l0
	st %l0, [%g3]
	add %g2, 4, %g2
	cmp %g2, %l2
	bl syspage_cpy
	add %g3, 4, %g3

	/* Flush TLB, I and D cache */
	sta %g0, [%g0] ASI_FLUSH_ALL

	/* Clear page tables */
	set ADDR_CTXTAB, %g2 /* %g2 = &pmap_common.pdir1 (phy) */
	set 0x5000, %g3
	clr %g1

clear_pdirs:
	subcc %g3, 8, %g3
	std %g0, [%g2]
	bnz clear_pdirs
	add %g2, 8, %g2

	/* Set context table pointer */
	set ADDR_CTXTAB, %g3
	set ADDR_PDIR1, %g4
	calc_ptd %g4, %g4
	st %g4, [%g3]
	srl %g3, 6, %g3
	sll %g3, 2, %g3
	set MMU_CTX_PTR, %g4
	sta %g3, [%g4] ASI_MMU_REGS

	/* Choose context 0 */
	set MMU_CTX, %g3
	sta %g0, [%g3] ASI_MMU_REGS

	/* Set up page table level 1 */

	set ADDR_PDIR1, %g1 /* %g1 = pmap_common.pdir1 */
	set ADDR_PDIR2, %g3 /* %g3 = pmap_common.pdir2 */

	/* V 0x40000000 -> P 0x40000000 */
	sethi %hi(ADDR_SRAM), %g2
	srl %g2, 24, %g2
	sll %g2, 2, %g2
	add %g1, %g2, %g4 /* %g4 = &pmap_common.pdir1[(ADDR_SRAM >> 24)] */
	calc_ptd %g3, %g5
	st %g5, [%g4] /* pmap_common.pdir1[(ADDR_SRAM >> 24)] = PTD(&pmap_common.pdir2[0]) */

	/* V 0xc0000000 -> P 0x40000000 */
	sethi %hi(VADDR_KERNEL), %g2
	srl %g2, 24, %g2
	sll %g2, 2, %g2
	/* %g1 = &pmap_common.pdir1 */
	add %g1, %g2, %g4 /* %g4 = &pmap_common.pdir1[(VADDR_KERNEL >> 24)] */
	st %g5, [%g4] /* pmap_common.pdir1[(VADDR_KERNEL >> 24)] = PTD(&pmap_common.pdir2[0]) */

	/* Setup page tables level 2 & 3 */

	set ADDR_PDIR2, %g1       /* %g1 = pmap_common.pdir2 */
	set ADDR_PDIR3, %g2       /* %g2 = pmap_common.pdir3 */
	sethi %hi(0x40000), %g3   /* 0x40000 = 256KB */
	sethi %hi(ADDR_SRAM), %g6
	or %g1, 0x100, %g5        /* end = 64 entries */
	set 0x1000, %l0

	/* for (int i = 0; i < 64; i++) {
	 *   pmap_common.pdir2[i] = PTD(&pmap_common.pdir3[i][0]);
	 *   for (int j = 0; j < 64; j++)
	 *     pmap_common.pdir3[i][j] = PTE(0x40000000 + (i * 256KB) + (j * 4KB));
	 * }
	 */

set_pdir2:
	add %g6, %g3, %g7
	calc_ptd %g2, %l2
	st  %l2, [%g1] /* pmap_common.pdir2[i] = &pmap_common.pdir3[i][0] */
set_pdir3:
	srl %g6, 12, %l1
	sll %l1, 8, %l1
	or %l1, ((1 << 7) | (0x7 << 2) | PAGE_ENTRY), %l1 /* cacheable, supervisor RWX */
	st %l1, [%g2]
	add %g6, %l0, %g6
	cmp %g6, %g7
	bne set_pdir3
	add %g2, 4, %g2

	add %g1, 4, %g1
	cmp %g1, %g5
	bne set_pdir2
	nop

	/* Enable MMU */
	mov 0x1, %g1
	sta %g1, [%g0] ASI_MMU_REGS

	/* Set up trap table */
	sethi %hi(_trap_table), %g1
	wr %g1, %tbr

	/* Set PSR to "supervisor", enable traps, disable interrupts, set CWP to 0 */
	mov %psr, %g1
	or %g1, (PSR_ET | PSR_S | PSR_PIL), %g1
	andn %g1, (PSR_CWP), %g1
	wr %g1, %psr
	nop
	nop
	nop

	wr %g0, 0x2, %wim

	/* Set stack pointer */
	clr %fp
	set VADDR_STACK, %sp
	sub %sp, 0x60, %sp

	set main, %g1
	call %g1
	mov %g0, %g1
.size _init, . - _init


.global _init_core
.type _init_core, #function
_init_core:
	/* Flush and enable cache */
	flush
	set 0x81000f, %g1
	sta %g1, [%g0] ASI_CACHE_CTRL

	/* Set context table pointer */
	set ADDR_CTXTAB, %g3
	set ADDR_PDIR1, %g4
	calc_ptd %g4, %g4
	st %g4, [%g3]
	srl %g3, 6, %g3
	sll %g3, 2, %g3
	set MMU_CTX_PTR, %g4
	sta %g3, [%g4] ASI_MMU_REGS

	clr %fp
	set VADDR_STACK, %sp

	/* Get CPU ID */
	rd %asr17, %g1
	srl %g1, 28, %g1
	sll %g1, 12, %g1
	add %sp, %g1, %sp
	sub %sp, 0x60, %sp

	/* Enable MMU */
	mov 0x1, %g1
	sta %g1, [%g0] ASI_MMU_REGS

	/* Set up trap table */
	sethi %hi(_trap_table), %g1
	wr %g1, %tbr

	mov %psr, %g1
	or %g1, (PSR_ET | PSR_S | PSR_PIL), %g1
	andn %g1, (PSR_CWP), %g1
	wr %g1, %psr

	/* Switch to virtual memory */
	sethi %hi(_vmem_switch), %g1
	jmp %g1 + %lo(_vmem_switch)
	wr %g0, 0x2, %wim

_vmem_switch:
	call hal_cpuInitCore
	nop

	/* Enable interrupts and wait to be scheduled */
	mov %psr, %g1
	andn %g1, (PSR_PIL), %g1
	wr %g1, %psr
	nop
	nop
	nop

_init_core_loop:
	wr %g0, %asr19
	ba _init_core_loop
	nop
.size _init_core, . - _init_core


.section ".bss"
.align 4
_hal_syspageCopied:
	.zero 0x400
.size _hal_syspageCopied, . - _hal_syspageCopied
