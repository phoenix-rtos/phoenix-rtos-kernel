/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupts handlers for sparcv8leon
 *
 * Copyright 2022, 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <config.h>
#include <arch/cpu.h>

#include "gaisler/grlib-tn-0018.h"

.extern hal_cpuKernelStack

.section ".text"
.align 4

.global _interrupts_dispatch
.type _interrupts_dispatch, #function

/* Interrupt handler
 * on entry:
 * %l0: psr
 * %l1: pc
 * %l2: npc
 * %l3: irq number
 */
_interrupts_dispatch:
	/* %g2, g3 used during manual window overflow */
	mov %g2, %l4
	mov %g3, %l5

	mov %wim, %g2
	/* check if we've just overflowed
	 * window overflow if wim == (1 << CWP)
	 * wim >> l0[4:0] - shift wim by CWP (lowest 5 bits from psr)
	 */
	srl %g2, %l0, %g3
	cmp %g3, 1

	bne irq_wovfl_done
	sll %g2, (NWINDOWS - 1), %g3

	/* calculate new wim: current %wim in %g2, %g3 is scratch */
	srl %g2, 1, %g2

	save
	wr %g2, %g3, %wim
	nop
	nop
	nop
	std %l0, [%sp + 0x00]
	std %l2, [%sp + 0x08]
	std %l4, [%sp + 0x10]
	std %l6, [%sp + 0x18]
	std %i0, [%sp + 0x20]
	std %i2, [%sp + 0x28]
	std %i4, [%sp + 0x30]
	std %fp, [%sp + 0x38]
	restore

irq_wovfl_done:
	/* check if we need to swap to kernel stack
	 * i.e. when PSR_PS is not set
	 */
	andcc %l0, PSR_PS, %g0
	bnz irq_no_kstack_switch

	set hal_cpuKernelStack, %l6
	/* Extract CPU ID and add offset */
	rd %asr17, %l7
	srl %l7, 28, %l7
	sll %l7, 2, %l7
	ld [%l6 + %l7], %l7
	ba irq_kstack_set
	sub %l7, CPU_CTX_SIZE, %sp

irq_no_kstack_switch:
	/* we came from kernel, make space for context */
	sub %fp, CPU_CTX_SIZE, %sp

irq_kstack_set:
	set PSR_EF, %g2
	andcc %l0, %g2, %g0
	bz irq_fpu_done
	nop

	FPU_SAVE

irq_fpu_done:
	/* Save context on kernel stack.
	 * Registers saved:
	 * %sp, %y, %psr, PC, nPC, %g1, %g2 (in %l4), %g3 (in %l5), %g4-%g7, %i0-%i7
	 */

	st  %sp, [%sp + 0x00] /* sp */
	rd  %y, %g2
	st  %g2, [%sp + 0x04] /* y */

	std %l0, [%sp + 0x08] /* psr, PC */
	st  %l2, [%sp + 0x10] /* nPC */
	st  %g1, [%sp + 0x14] /* g1 */
	std %l4, [%sp + 0x18] /* g2, g3 */
	std %g4, [%sp + 0x20] /* g4, g5 */
	std %g6, [%sp + 0x28] /* g6, g7 */

	/* input registers here are the outputs of the interrupted window */

	std %i0, [%sp + 0x30] /* i0, i1 */
	std %i2, [%sp + 0x38] /* i2, i3 */
	std %i4, [%sp + 0x40] /* i4, i5 */
	std %fp, [%sp + 0x48] /* fp (task's sp), i7 */

	mov %sp, %l7
	sub %sp, 0x60, %sp

	mov %l7, %o1 /* (cpu_context_t *) */

	mov %l0, %g2
	mov 1, %g4
	sll %g4, %g2, %g3 /* %g3 = 1 << %g2[4:0] (CWP) */

	sethi %hi(_interrupts_saveContext), %g5
	jmpl %g5 + %lo(_interrupts_saveContext), %g1 /* clobbers %g1, %g3, %l2 */
	rd %wim, %g4

	/* enable traps, disable interrupts, restore CWP */
	or %g2, (PSR_PIL | PSR_ET), %g2
	wr %g2, %psr

	and %g2, PSR_CWP, %g2
	add %g2, 1, %g2
	cmp %g2, NWINDOWS
	bne irq_cwp_done0
	mov 1, %g3

	mov 0, %g2

irq_cwp_done0:
	/* set %wim to 1 << %g2 (CWP + 1) */
	sll %g3, %g2, %g2
	wr %g2, %wim
	nop
	nop
	nop

	/* void interrupts_dispatch(unsigned int irq, cpu_context_t *) */
	call interrupts_dispatch
	mov %l3, %o0 /* irq */

	/* disable traps */
#ifdef LEON3_USE_PWR
	pwr 0, %psr
	nop
	nop
	nop
#else
	ta 0xb
#endif

	/* l7 still points to bottom of context */
	mov %l7, %sp

	/* check if we're going to switch context (sp != *(sp)) */
	ld [%sp], %g2
	cmp %sp, %g2

	MULTILOCK_CLEAR

	be irq_no_switch
	nop

	/* %g2 points to the new task's context */

	mov %g0, %wim /* we don't need it now */
	ld [%g2 + 0x08], %g1
	nop
	andn %g1, PSR_ET, %g1 /* leave traps disabled */

	/* Set %psr of the new task */
	wr %g1, %psr
	/* no delay needed, we're using global registers */

	sethi %hi(_interrupts_restoreContext), %g5
	jmpl %g5 + %lo(_interrupts_restoreContext), %g1
	nop

	/* check CWP overflow (same as before) */
	and %g2, PSR_CWP, %g2
	add %g2, 1, %g2
	cmp %g2, NWINDOWS
	bne irq_cwp_done
	mov 1, %g3

	mov 0, %g2

irq_cwp_done:
	/* set %wim to 1 << %g2 (CWP + 2) */
	sll %g3, %g2, %g2
	mov %g2, %wim

	/* restore %g1, %g2, %g3 */
	ld  [%sp + 0x14], %g1

	andn %l0, PSR_ET, %l0

	ba irq_return
	ldd [%sp + 0x18], %g2


irq_no_switch:
	/* restore current window */
	ld  [%sp + 0x04], %g1 /* y */
	ldd [%sp + 0x08], %l0 /* psr, PC */
	wr  %g1, %y
	ld  [%sp + 0x10], %l2 /* nPC */
	ld  [%sp + 0x14], %g1
	ldd [%sp + 0x18], %g2
	ldd [%sp + 0x20], %g4
	ldd [%sp + 0x28], %g6

	ldd [%sp + 0x30], %i0
	ldd [%sp + 0x38], %i2
	ldd [%sp + 0x40], %i4
	ldd [%sp + 0x48], %fp

	/* Check if restore would cause window underflow.
	 * After restore: CWP = CWP + 1 (mod NWINDOWS)
	 * i.e. wim >> (CWP + 1) == 1
	 */

	and	%l0, PSR_CWP, %l5
	add %l5, 1, %l5
	cmp %l5, NWINDOWS
	bne irq_cwp_done2
	rd %wim, %l4

	/* we'd end up in non-existent window #NWINDOWS, it means it's #0 */
	mov 0, %l5

irq_cwp_done2:
	/* l4 = wim, l5 = CWP + 1 (mod NWINDOWS)
	 * check if wim >> (CWP + 1) == 1 (window underflow)
	 */
	srl %l4, %l5, %l6
	cmp %l6, 1
	bne irq_return
	/* uses the delay slot
	 * calculate new wim
	 * %l4 = current %wim
	 * wim = (wim << 1) ^ (wim >> (NWINDOWS - 1))
	 */
	sll %l4, 1, %l5
	srl %l4, (NWINDOWS - 1), %l4
	wr %l4, %l5, %wim
	nop
	nop
	nop
	restore
	ldd [%sp + 0x00], %l0
	ldd [%sp + 0x08], %l2
	ldd [%sp + 0x10], %l4
	ldd [%sp + 0x18], %l6
	ldd [%sp + 0x20], %i0
	ldd [%sp + 0x28], %i2
	ldd [%sp + 0x30], %i4
	ldd [%sp + 0x38], %fp
	save

irq_return:
	TN_0018_WAIT_ICACHE(%l3, %l4)

	wr %l0, %psr
	nop
	nop
	nop

	TN_0018_FIX(%l3, %l4)

	jmp %l1
	rett %l2
.size _interrupts_dispatch, . - _interrupts_dispatch
