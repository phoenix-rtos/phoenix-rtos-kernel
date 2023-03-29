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

.section ".text"
.align 4

.global _interrupts_save_context
.type _interrupts_save_context, #function
_interrupts_save_context:
	/* This function saves all valid windows on stack.
	 * Before calling, following registers must be set:
	 * %g1 - call address
	 * %g3 - 1 << (CWP + 1) (mod NWINDOWS)
	 * %g4 - wim
	 *
	 * Clobbers %g3, %g5
	 */

save_context_loop:
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
	std %i6, [%sp + 0x38]

	/* calculate new CWP mask */
	sll %g3, 1, %g5
	srl %g3, (NWINDOWS - 1), %g3
	ba save_context_loop
	or %g3, %g5, %g3

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
	/* don't restore %i6 (task's sp) */
	ld  [%sp + 0x4c], %i7

	add %sp, 0x50, %fp

	/* switch window to the task's window and restore context */
	restore

	ldd [%sp + 0x00], %l0
	ldd [%sp + 0x08], %l2
	ldd [%sp + 0x10], %l4
	ldd [%sp + 0x18], %l6
	ldd [%sp + 0x20], %i0
	ldd [%sp + 0x28], %i2
	ldd [%sp + 0x30], %i4
	ldd [%sp + 0x38], %i6

	rd %psr, %g2

	/* go back to handler's window */
	save

	jmpl %g1 + 8, %g0
	nop
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
	std %i6, [%sp + 0x38]
	restore

irq_wovfl_done:
	/* save context on stack - here is saved only part of thread context
	 * actual saving is done only if we're switching context
	 *
	 * Registers saved:
	 * %sp, %y, %psr, PC, nPC, %g1, %g2 (in %l4), %g3 (in %l5) %g4, %g5, %i0-%i7
	 */

	sub %fp, 0x50, %sp

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
	std %i6, [%sp + 0x48] /* i6, i7 */

	/* ends at %sp + 0x50 */

	mov %sp, %l1 /* save pointer to context */

	/* check if we need to swap to kernel stack
	 * i.e. when PSR_PS is not set
	 */
	andcc %l0, PSR_PS, %g0
	bnz irq_kstack_set
	nop

	/* swap to kernel stack */

irq_kstack_set:
	/* allocate stack frame */
	mov %sp, %fp
	sub %sp, 0x60, %sp

	mov %l1, %o1 /* cpu_context_t * */

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

	/* back to task context */
	mov %fp, %sp
	add %sp, 0x50, %fp

	/* check if we're going to switch context (l1 != *(l1)) */
	ld [%l1], %g2
	cmp %l1, %g2
	be irq_no_switch
	nop

	/* this code should be common for hal_cpuReschedule */

	/* we're switching, save used register windows on stack
	 * and load only the window we'll be returning to.
	 * The rest will be restored on window underflows.
	 */

	/* CWP ranges from 0 to (NWINDOWS - 1) */
	rd %psr, %g3
	and %g3, PSR_CWP, %g3
	add %g3, 1, %g3
	cmp %g3, NWINDOWS
	bne irq_cwp_done
	nop
	/* we'd end up in non-existent window #31, it means it's #0 */
	mov 0, %g3

irq_cwp_done:
	/* Current state of registers:
	 * %g2 - %sp of new task
	 * %g3 - CWP + 1 (mod NWINDOWS)
	 * freely usable: %g4, %g5
	 */

	/* set bit in register %g3, which corresponds to CWP + 1
	 * %g3 = 1 << %g3 (CWP + 1)
	 */
	mov 1, %g4
	sll %g4, %g3, %g3

	rd %wim, %g4
	/* wim has one bit set - it's the invalid window
	 * we need to do 'restore' until we reach it
	 */

	/* save context on stack */
	set _interrupts_save_context, %g5
	jmpl %g5, %g1 /* clobbers %g1, %g3, %g5 */
	nop

	/* At this point, we've saved all registers that the previous
	 * task used, and we're ready to switch to the new task.
	 *
	 * %g2 points to the new task's context.
	 */

	mov %g0, %wim /* we don't need it now */

	/* Set %psr of the new task.
	 * This will cause window to be switched
	 * to the window in interrupt handler.
	 */

	ld [%g2 + 0x08], %g1
	andn %g1, (PSR_ET | PSR_PIL), %g1 /* disable traps, enable interrupts */
	wr %g1, %psr
	nop
	nop
	nop

	set _interrupts_restore_context, %g5
	jmpl %g5, %g1
	nop

	/* check CWP overflow (same as before) */
	and %g2, PSR_CWP, %g2
	add %g2, 1, %g2
	cmp %g2, NWINDOWS
	bne irq_cwp_done2
	nop
	mov 0, %g2

irq_cwp_done2:
	/* set %wim to 1 << %g2 (CWP + 1) */
	mov 1, %g3
	sll %g3, %g2, %g2
	mov %g2, %wim

	/* restore %g1, %g2, %g3 */
	ld  [%sp + 0x14], %g1
	ldd [%sp + 0x18], %g2

	ba irq_switch_return
	nop


irq_no_switch:
	/* restore current window */
	ld  [%sp + 0x04], %g1 /* y */
	wr  %g1, %y
	ldd [%sp + 0x08], %l0 /* psr, PC */
	ld  [%sp + 0x10], %l2 /* nPC */
	ld  [%sp + 0x14], %g1
	ldd [%sp + 0x18], %g2
	ldd [%sp + 0x20], %g4
	ldd [%sp + 0x28], %g6

	ldd [%sp + 0x30], %i0
	ldd [%sp + 0x38], %i2
	ldd [%sp + 0x40], %i4
	ldd [%sp + 0x48], %i6

	/* Check if restore would cause window underflow.
	 * After restore: CWP = CWP + 1 (mod NWINDOWS)
	 * i.e. wim >> (CWP + 1) == 1
	 */
	rd %wim, %l4
	and	%l0, PSR_CWP, %l5
	add %l5, 1, %l5
	cmp %l5, NWINDOWS
	bne irq_cwp_done3
	nop
	/* we'd end up in non-existent window #31, it means it's #0 */
	mov 0, %l5

irq_cwp_done3:
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
	ldd [%sp + 0x38], %i6
	save

irq_return:
	wr %l0, %psr
	nop
	nop
	nop

irq_switch_return:
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
	save %sp, -0xb0, %sp

	/* disable interrupts */
	ta 0x09
	/* this is voluntary reschedule,
	 * no need to store caller-saves registers
	 * (outputs of previous window and g1-g5)
	 */
	/* disable traps */
	pwr 0, %psr
	nop
	nop
	nop
	rd %psr, %g2

	add %sp, 0x60, %l1    /* save pointer to context */
	st  %l1, [%l1 + 0x00]
	st  %g0, [%l1 + 0x04]
	st  %g2, [%l1 + 0x08] /* psr */
	add %i7, 0x8, %g2
	st  %g2, [%l1 + 0x0c] /* pc */
	add %g2, 0x4, %g2
	st  %g2, [%l1 + 0x10] /* npc */

	std %g6, [%l1 + 0x28]

	st  %g0, [%l1 + 0x30] /* default return value */

	/* This time, unlike in interrupts,
	 * we know we'll be switching context so save windows now.
	 * Save used register windows on stack
	 * and load only the window we'll be returning to.
	 * The rest will be restored on window underflows.
	 */

	/* CWP ranges from 0 to (NWINDOWS - 1) */
	rd %psr, %g2
	and %g2, PSR_CWP, %g3
	add %g3, 1, %g3
	cmp %g3, NWINDOWS
	bne r_cwp_done
	nop
	/* we'd end up in non-existent window #31, it means it's #0 */
	mov 0, %g3

r_cwp_done:
	/* Current state of registers:
	 * %l1 - %sp of old task
	 * %g2 - %psr
	 * %g3 - CWP + 1 (mod NWINDOWS)
	 * freely usable: %g4, %g5
	 */

	/* set bit in register %g3, which corresponds to CWP + 1
	 * %g3 = 1 << %g3 (CWP + 1)
	 */
	mov 1, %g4
	sll %g4, %g3, %g3

	rd %wim, %g4

	set _interrupts_save_context, %g5
	jmpl %g5, %g1 /* clobbers %g1, %g3, %g5 */
	nop

	/* restore psr so it's the same as before context save,
	 * enable traps (we have free space on stack)
	 */
	or %g2, PSR_ET, %g2
	wr %g2, %psr
	nop
	nop
	nop

	/* check if spinlock is not NULL */
	cmp %i0, %g0
	beq r_spinlock_done
	nop

r_spinlock:
	/* clear spinlock */
	stb %g0, [%i0 + 0x0c]
	stbar
	ldub [%i0 + 0x0c], %o0
	cmp %o0, %g0
	bne r_spinlock
	nop

r_spinlock_done:
	clr %o0
	mov %l1, %o1 /* cpu_context_t * */
	call threads_schedule
	clr %o2

	pwr 0, %psr
	nop
	nop
	nop

	ld [%l1], %g2
	/* context is switched, %g2 points to new context */

	mov %g0, %wim /* we don't need it now */

	/* Set %psr of the new task.
	 * This will cause window to be switched
	 * so that the new task's window is CWP + 1.
	 */

	ld [%g2 + 0x08], %g1
	andn %g1, PSR_ET, %g1 /* disable traps */
	or %g1, PSR_PIL, %g1  /* disable interrupts */
	wr %g1, %psr
	nop
	nop
	nop

	set _interrupts_restore_context, %g5
	jmpl %g5, %g1
	nop

	/* check CWP overflow (same as before) */
	and %g2, PSR_CWP, %g2
	add %g2, 1, %g2
	cmp %g2, NWINDOWS
	bne r_cwp_done2
	nop
	mov 0, %g2

r_cwp_done2:
	/* set %wim to 1 << %g2 (CWP + 1) */
	mov 1, %g3
	sll %g3, %g2, %g2
	mov %g2, %wim

	/* restore %g1, %g2, %g3 */
	ld  [%sp + 0x14], %g1
	ldd [%sp + 0x18], %g2

	/* enable traps */
	rd %psr, %l0
	or %l0, PSR_ET, %l0
	wr %l0, %psr
	nop
	nop
	nop

	/* Return from hal_cpuReschedule is done through a trap.
	 * It allows to safely resume thread execution even
	 * if the thread was interrupted in the delay slot
	 * of a branch instruction (both PC and nPC have to be updated).
	 */
	ta 0x0b
	/* no return back here */
.size hal_cpuReschedule, . - hal_cpuReschedule


/* void hal_jmp(void *f, void *kstack, void *stack, int argc) */
.global	hal_jmp
.type	hal_jmp, #function
hal_jmp:
	ta 0x09
	mov %o0, %o4
	mov %o2, %o5
	cmp %o5, %g0 /* stack != NULL */
	bne 2f
	nop
	mov %o1, %sp /* sp = kstack */
	subcc %o3, 1, %o3
	bneg 1f
	nop
	subcc %o3, 1, %o3
	bneg 1f
	ld [%sp], %o0
	subcc %o3, 1, %o3
	bneg 1f
	ld [%sp + 4], %o1
	subcc %o3, 1, %o3
	bneg 1f
	ld [%sp + 8], %o2
	ld [%sp + 12], %o3
1:
	andn %sp, 0x7, %sp
	sub %sp, 0x60, %sp
	ta  0x0a
	call %o4
	nop
2:
	subcc %o3, 1, %o3
	bneg 3f
	nop
	subcc %o3, 1, %o3
	bneg 3f
	ld [%o5], %o0
	subcc %o3, 1, %o3
	bneg 3f
	ld [%o5 + 4], %o1
	subcc %o3, 1, %o3
	bneg 3f
	ld [%o5 + 8], %o2
	ld [%o5 + 12], %o3
3:
	andn %o5, 0x7, %sp
	sub %sp, 0x70, %sp
	ta  0x0a
	call %o4
	nop
.size hal_jmp, . - hal_jmp


/* void hal_longjmp(cpu_context_t *ctx); */
.global	hal_longjmp
.type	hal_longjmp, #function
hal_longjmp:
	/* disable interrupts */
	ta 0x09
	ld [%o0], %g1
	ld [%o0 + 0x08], %g2 /* psr */
	or %g2, PSR_PIL, %g2 /* disable interrupts */
	/* writing %psr might cause window switch */
	wr %g2, %psr
	nop
	nop
	nop
	mov %g1, %sp
	ld [%sp + 0x04], %g1
	wr %g1, %y

	ld  [%sp + 0x0c], %o7 /* pc */
	ld  [%sp + 0x14], %g1
	ldd [%sp + 0x18], %g2
	ldd [%sp + 0x20], %g4
	ldd [%sp + 0x28], %g6

	ldd [%sp + 0x30], %o0
	ldd [%sp + 0x38], %o2
	ldd [%sp + 0x40], %o4

	ldd [%sp + 0x50], %l0
	ldd [%sp + 0x58], %l2
	ldd [%sp + 0x60], %l4
	ldd [%sp + 0x68], %l6

	ldd [%sp + 0x70], %i0
	ldd [%sp + 0x78], %i2
	ldd [%sp + 0x80], %i4
	ldd [%sp + 0x88], %i6

	/* enable interrupts */
	ta  0x0a
	add %sp, 0x90, %sp

	jmp %o7
	ld  [%sp - 0x44], %o7
.size hal_longjmp, . - hal_longjmp
