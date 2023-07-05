/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupts handlers for sparcv8leon3
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

.extern _end
.extern hal_cpuKernelStack

.section ".text"
.align 4

.global _interrupts_save_context
.type _interrupts_save_context, #function
_interrupts_save_context:
	/* This function saves all valid windows on stack.
	 * Before calling, following registers must be set:
	 * %g1 - call address
	 * %g3 - bit set for CWP
	 * %g4 - wim
	 *
	 * Clobbers %g1, %g3
	 */

save_context_loop:
	sll %g3, 1, %l0
	srl %g3, (NWINDOWS - 1), %g3
	or %g3, %l0, %g3
	/* check if restore won't underflow */
	andcc %g3, %g4, %g0
	bnz save_context_done
	nop

	/* if not, advance to next window */
	restore

	/* save window on current %sp */
	std %l0, [%sp + 0x00]
	std %l2, [%sp + 0x08]
	std %l4, [%sp + 0x10]
	std %l6, [%sp + 0x18]
	std %i0, [%sp + 0x20]
	std %i2, [%sp + 0x28]
	std %i4, [%sp + 0x30]
	std %fp, [%sp + 0x38]

	ba save_context_loop
	nop

save_context_done:
	jmpl %g1 + 8, %g0
	nop
.size _interrupts_save_context, . - _interrupts_save_context


.global _interrupts_restore_context
.type _interrupts_restore_context, #function
_interrupts_restore_context:
	/* This function restores current window (except %g1, %g2, %g3)
	 * and first task's window.
	 * Before calling, following registers must be set:
	 * %g1 - call address
	 * %g2 - pointer to context
	 *
	 * On return: task's %psr in %g2
	 */

	/* switch to new task's stack */
	mov %g2, %sp

	/* restore current window */
	ld  [%sp + 0x04], %g2 /* y */
	wr  %g2, %y
	ldd [%sp + 0x08], %l0 /* psr, PC */
	ld  [%sp + 0x10], %l2 /* nPC */

	/* %g1, %g2, %g3 must be restored later */

	ldd [%sp + 0x20], %g4
	ldd [%sp + 0x28], %g6

	ldd [%sp + 0x30], %i0
	ldd [%sp + 0x38], %i2
	ldd [%sp + 0x40], %i4
	ldd [%sp + 0x48], %fp

	/* switch window to the task's window and restore context */
	restore

	ldd [%sp + 0x00], %l0
	ldd [%sp + 0x08], %l2
	ldd [%sp + 0x10], %l4
	ldd [%sp + 0x18], %l6
	ldd [%sp + 0x20], %i0
	ldd [%sp + 0x28], %i2
	ldd [%sp + 0x30], %i4
	ldd [%sp + 0x38], %fp

	rd %psr, %g2

	/* go back to handler's window */
	save

	jmpl %g1 + 8, %g0
	nop
.size _interrupts_restore_context, . - _interrupts_restore_context


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

	sethi %hi(hal_cpuKernelStack), %l6
	ld [%l6 + %lo(hal_cpuKernelStack)], %l7
	ba irq_kstack_set
	sub %l7, 0x50, %sp

irq_no_kstack_switch:
	/* we came from kernel, make space for context */
	sub %fp, 0x50, %sp

irq_kstack_set:
	/* Save context on kernel stack - we have enough space for 1 window.
	 * Here only a part of thread context is saved,
	 * all windows are saved only if we're switching context.
	 *
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

	/* enable traps, disable interrupts */
	or %l0, (PSR_PIL | PSR_ET), %l0
	wr %l0, %psr
	nop
	nop
	nop

	/* void interrupts_dispatch(unsigned int irq, cpu_context_t *) */
	call interrupts_dispatch
	mov %l3, %o0 /* irq */

	/* disable traps */
	pwr 0, %psr
	nop
	nop
	nop

	/* l7 still points to bottom of context */
	mov %l7, %sp

	/* check if we're going to switch context (sp != *(sp)) */
	ld [%sp], %g2
	cmp %sp, %g2
	be irq_no_switch
	nop

	/* We're switching, save used register windows on stack
	 * and load only the window we'll be returning to.
	 * The rest will be restored on window underflows.
	 */

	rd %psr, %g3
	and %g3, PSR_CWP, %g3

	/* Current state of registers:
	 * %g2 - %sp of new task
	 * %g3 - CWP
	 * freely usable: %g4, %g5
	 */

	/* set bit in register %g3, which corresponds to CWP
	 * %g3 = 1 << %g3 (CWP)
	 */
	mov 1, %g4
	sll %g4, %g3, %g3

	/* save context on stack */
	sethi %hi(_interrupts_save_context), %g5
	jmpl %g5 + %lo(_interrupts_save_context), %g1 /* clobbers %g1, %g3 */
	rd %wim, %g4

	/* At this point, we've saved all registers that the previous
	 * task used, and we're ready to switch to the new task.
	 *
	 * %g2 points to the new task's context.
	 */

	mov %g0, %wim /* we don't need it now */
	ld [%g2 + 0x08], %g1
	nop
	andn %g1, PSR_ET, %g1 /* leave traps disabled */

	/* Set %psr of the new task.
	 * This will cause window to be switched
	 * to the window in interrupt handler.
	 */

	wr %g1, %psr
	/* no delay needed, we're using global registers */

	sethi %hi(_interrupts_restore_context), %g5
	jmpl %g5 + %lo(_interrupts_restore_context), %g1
	nop

	/* check CWP overflow (same as before) */
	and %g2, PSR_CWP, %g2
	add %g2, 1, %g2
	cmp %g2, NWINDOWS
	bne irq_cwp_done
	nop
	mov 0, %g2

irq_cwp_done:
	/* set %wim to 1 << %g2 (CWP + 2) */
	mov 1, %g3
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

	/* we'd end up in non-existent window #31, it means it's #0 */
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
	wr %l0, %psr
	nop
	nop
	nop

	jmp %l1
	rett %l2
.size _interrupts_dispatch, . - _interrupts_dispatch


/* Disable/Enable interrupts
 * on entry:
 * %psr in %l0, PC in %l1, nPC in %l2
 */

.global _interrupts_disable
.type _interrupts_disable, #function
_interrupts_disable:
	or %l0, PSR_PIL, %l0
	wr %l0, %psr
	nop
	nop
	nop
	jmp %l2
	rett %l2 + 4
.size _interrupts_disable, . - _interrupts_disable


.global _interrupts_enable
.type _interrupts_enable, #function
_interrupts_enable:
	andn %l0, PSR_PIL, %l0
	wr %l0, %psr
	nop
	nop
	nop
	jmp %l2
	rett %l2 + 4
.size _interrupts_enable, . - _interrupts_enable


/* int hal_cpuReschedule(struct _spinlock_t *spinlock, spinlock_ctx_t *scp) */
.global	hal_cpuReschedule
.type	hal_cpuReschedule, #function
hal_cpuReschedule:
	save %sp, -0x50, %sp

	pwr 0, %psr
	nop
	nop
	nop

	/* this is voluntary reschedule,
	 * no need to store caller-saves registers
	 * (outputs of previous window and g1-g5)
	 */

	rd %psr, %g2

	or %g2, PSR_PS, %g2

	mov %sp, %l1    /* save pointer to context */

	st  %sp, [%sp + 0x00]
	st  %g0, [%sp + 0x04]
	st  %g2, [%sp + 0x08] /* psr */
	add %i7, 0x8, %g3
	st  %g3, [%sp + 0x0c] /* pc */
	add %g3, 0x4, %g3
	st  %g3, [%sp + 0x10] /* npc */
	std %g6, [%sp + 0x28]
	st  %g0, [%sp + 0x30] /* default return value */
	st  %fp, [%sp + 0x48]

	sub %sp, 0x60, %sp

	/* check if spinlock is not NULL */
	cmp %i0, %g0
	beq r_spinlock_done
	nop

r_spinlock:
	/* clear spinlock */
	stbar
	stub %g0, [%i0 + 0x0c]

r_spinlock_done:
	/* disable interrupts, enable traps */
	or %g2, (PSR_PIL | PSR_ET), %g2
	wr %g2, %psr
	nop
	nop
	nop

	clr %o0
	mov %l1, %o1 /* cpu_context_t * */
	call threads_schedule
	clr %o2

	/* disable traps */
	pwr 0, %psr
	nop
	nop
	nop

	ld [%l1], %g2 /* ctx pointer */

	rd %psr, %g3
	and %g3, PSR_CWP, %g3

	/* set bit in register %g3, which corresponds to CWP
	 * %g3 = 1 << %g3 (CWP)
	 */
	mov 1, %g4
	sll %g4, %g3, %g3

	sethi %hi(_interrupts_save_context), %g5
	jmpl %g5 + %lo(_interrupts_save_context), %g1 /* clobbers %g1, %g3 */
	rd %wim, %g4

	/* Set %psr of the new task.
	 * This will cause window to be switched
	 * so that the new task's window is CWP + 1.
	 */

	mov %g0, %wim /* we don't need it now */
	ld [%g2 + 0x08], %g1
	nop
	andn %g1, PSR_ET, %g1 /* disable traps */
	wr %g1, %psr

	sethi %hi(_interrupts_restore_context), %g5
	jmpl %g5 + %lo(_interrupts_restore_context), %g1
	nop

	/* check CWP overflow (same as before) */
	and %g2, PSR_CWP, %g2
	add %g2, 1, %g2
	cmp %g2, NWINDOWS
	bne r_cwp_done
	nop
	mov 0, %g2

r_cwp_done:
	/* set %wim to 1 << %g2 (CWP + 1) */
	mov 1, %g3
	sll %g3, %g2, %g2
	wr %g2, %wim
	nop

	/* restore %g1, %g2, %g3 */
	ld  [%sp + 0x14], %g1
	ldd [%sp + 0x18], %g2

	andn %l0, PSR_ET, %l0
	wr %l0, %psr
	nop
	nop
	nop

	/* Set both PC and nPC */
	jmp %l1
	rett %l2
.size hal_cpuReschedule, . - hal_cpuReschedule


/* void hal_jmp(void *f, void *kstack, void *stack, int argc) */
.global	hal_jmp
.type	hal_jmp, #function
hal_jmp:
	save %sp, -0x60, %sp
	pwr 0, %psr
	nop
	nop
	nop
	/* calculate new wim */
	rd %psr, %l0
	and %l0, PSR_CWP, %l1
	add %l1, 1, %l1
	cmp %l1, NWINDOWS
	bne 1f
	nop

	mov 0, %l1
1:
	mov 1, %l2
	sll %l2, %l1, %l1
	wr %l1, %wim
	nop
	nop
	nop

	cmp %i2, %g0 /* stack != NULL */
	bne 3f
	nop
	mov %i1, %fp /* fp = kstack */
	subcc %i3, 1, %i3
	bneg 2f
	nop
	subcc %i3, 1, %i3
	bneg 2f
	ld [%fp], %o0
	subcc %i3, 1, %i3
	bneg 2f
	ld [%fp + 4], %o1
	subcc %i3, 1, %i3
	bneg 2f
	ld [%fp + 8], %o2
	ld [%fp + 12], %o3
2:
	andn %fp, 0x7, %fp
	sub %fp, 0x60, %sp
	wr %l0, PSR_ET, %psr
	nop
	nop
	nop
	call %i0
	nop
3:
	subcc %i3, 1, %i3
	bneg 4f
	nop
	subcc %i3, 1, %i3
	bneg 4f
	ld [%i2], %o0
	subcc %i3, 1, %i3
	bneg 4f
	ld [%i2 + 4], %o1
	subcc %i3, 1, %i3
	bneg 4f
	ld [%i2 + 8], %o2
	ld [%i2 + 12], %o3
4:
	andn %i2, 0x7, %fp
	sub %fp, 0x60, %sp
	andn %l0, (PSR_S | PSR_PIL), %l0
	wr %l0, PSR_ET, %psr
	nop
	nop
	nop
	mov %i2, %o0
	call %i0
	nop
.size hal_jmp, . - hal_jmp


/* void hal_longjmp(cpu_context_t *ctx); */
.global	hal_longjmp
.type	hal_longjmp, #function
hal_longjmp:
	/* disable interrupts */
	ta 0x09
	mov %o0, %g1
	ld [%o0 + 0x08], %g2 /* psr */
	or %g2, PSR_PIL, %g3 /* disable interrupts */
	mov %g0, %wim
	nop
	nop
	nop
	/* writing psr might cause window switch */
	wr %g3, %psr
	nop
	nop
	nop
	mov %g1, %sp

	/* restore context - no need to load caller-saves */
	ld  [%sp + 0x0c], %o7 /* pc */
	ldd [%sp + 0x28], %g6

	ld  [%sp + 0x30], %o0

	ldd [%sp + 0x50], %l0
	ldd [%sp + 0x58], %l2
	ldd [%sp + 0x60], %l4
	ldd [%sp + 0x68], %l6

	ldd [%sp + 0x70], %i0
	ldd [%sp + 0x78], %i2
	ldd [%sp + 0x80], %i4
	ldd [%sp + 0x88], %fp

	/* calculate new wim */
	and %g2, PSR_CWP, %g1
	add %g1, 1, %g1
	cmp %g1, NWINDOWS
	bne 5f
	nop
	mov 0, %g1
5:
	mov 1, %g3
	sll %g3, %g1, %g1
	wr %g1, %wim
	nop
	nop
	nop

	/* restore psr */
	wr %g2, PSR_ET, %psr
	nop
	nop
	nop

	jmp %o7
	add %sp, 0x60, %sp
.size hal_longjmp, . - hal_longjmp
