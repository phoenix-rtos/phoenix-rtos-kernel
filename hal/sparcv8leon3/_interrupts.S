/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupts handlers for sparcv8leon3
 *
 * Copyright 2022 Phoenix Systems
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
.global _interrupts_dispatch
.type _interrupts_dispatch, #function

/*
 * on entry:
 * %l0: psr
 * %l1: pc
 * %l2: npc
 * %l3: irq number
 */
_interrupts_dispatch:
	restore

	sub %sp, 0x90, %sp

	st %sp, [%sp + 0x00]

	std %o0, [%sp + 0x10]
	std %o2, [%sp + 0x18]
	std %o4, [%sp + 0x20]
	std %o6, [%sp + 0x28]

	std %l0, [%sp + 0x30]
	std %l2, [%sp + 0x38]
	std %l4, [%sp + 0x40]
	std %l6, [%sp + 0x48]

	std %i0, [%sp + 0x50]
	std %i2, [%sp + 0x58]
	std %i4, [%sp + 0x60]
	std %i6, [%sp + 0x68]

	st %g1, [%sp + 0x70]
	st %g2, [%sp + 0x74]
	st %g3, [%sp + 0x78]
	st %g4, [%sp + 0x7c]
	st %g5, [%sp + 0x80]
	st %g6, [%sp + 0x84]
	st %g7, [%sp + 0x88]

	rd  %y, %g1
	st  %g1,  [%sp + 0x8c] /* y */

	save

	/* save %psr, %pc, %npc */
	st %l0, [%i6 + 0x04]
	st %l1, [%i6 + 0x08]
	st %l2, [%i6 + 0x0c]

	/* Enable traps, disable interrupts */
	or %l0, PSR_PIL, %l4
	wr %l4, PSR_ET, %psr
	nop
	nop
	nop

	mov %l3, %i0

	restore /* handle everything in the same window */

	sub %sp, 0x60, %sp
	/* void interrupts_dispatch(unsigned int irq, cpu_context_t *) */
	call interrupts_dispatch
	add %sp, 0x60, %o1

	save
	/* Disable traps, we can't let a trap use the stack now */
	pwr 0, %psr
	nop
	nop
	nop

	/* restore context of interrupted window */
	restore

	ld [%sp + 0x60], %sp

	ld [%sp + 0x8c], %g1
	wr %g1, %g0, %y

	ld [%sp + 0x88], %g7
	ld [%sp + 0x84], %g6
	ld [%sp + 0x80], %g5
	ld [%sp + 0x7c], %g4
	ld [%sp + 0x78], %g3
	ld [%sp + 0x74], %g2
	ld [%sp + 0x70], %g1

	ldd [%sp + 0x68], %i6
	ldd [%sp + 0x60], %i4
	ldd [%sp + 0x58], %i2
	ldd [%sp + 0x50], %i0

	ldd [%sp + 0x48], %l6
	ldd [%sp + 0x40], %l4
	ldd [%sp + 0x38], %l2
	ldd [%sp + 0x30], %l0

	ld [%sp + 0x2c], %o7
	ldd [%sp + 0x20], %o4
	ldd [%sp + 0x18], %o2
	ldd [%sp + 0x10], %o0

	add %sp, 0x90, %sp

	save

	sub %i6, 0x90, %l6
	ld [%l6 + 0x08], %l1
	ld [%l6 + 0x0c], %l2
	ld [%l6 + 0x04], %l0

	/* restore %psr, don't change windows now! */
	andn %l0, (PSR_CWP | PSR_ET | PSR_PIL), %l0
	or %l0, 0x1e, %l0
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
	/* disable interrupts */
	ta 0x09
	/* disable traps */
	pwr 0, %psr
	nop
	nop
	nop
	sub %sp, 0x90, %sp
	st  %sp, [%sp + 0x00]

	st  %g0, [%sp + 0x10] /* default return value */
	st  %o1, [%sp + 0x14]
	std %o2, [%sp + 0x18]
	std %o4, [%sp + 0x20]
	std %o6, [%sp + 0x28]

	std %l0, [%sp + 0x30]
	std %l2, [%sp + 0x38]
	std %l4, [%sp + 0x40]
	std %l6, [%sp + 0x48]

	std %i0, [%sp + 0x50]
	std %i2, [%sp + 0x58]
	std %i4, [%sp + 0x60]
	std %i6, [%sp + 0x68]

	st %g1, [%sp + 0x70]
	st %g2, [%sp + 0x74]
	st %g3, [%sp + 0x78]
	st %g4, [%sp + 0x7c]
	st %g5, [%sp + 0x80]
	st %g6, [%sp + 0x84]
	st %g7, [%sp + 0x88]

	add %o7, 0x8, %o7
	st  %o7, [%sp + 0x08] /* pc */
	add %o7, 0x4, %o7
	st  %o7, [%sp + 0x0c] /* npc */

	rd %y, %g1
	st %g1,  [%sp + 0x8c] /* y */

	rd %psr, %g1
	st %g1,  [%sp + 0x04] /* psr */

	/* check if spinlock is not NULL */
	cmp %o0, %g0
	beq spinlock_done
	sub %sp, 0x60, %sp

spinlock:
	/* clear spinlock */
	stb %g0, [%o0 + 0x0c]
	stbar
	ldub [%o0 + 0x0c], %g1
	cmp %g1, %g0
	bne spinlock
	nop

spinlock_done:
	clr %o0
	add %sp, 0x60, %o1
	call threads_schedule
	clr %o2

	ld [%sp + 0x60], %sp

	ld [%sp + 0x8c], %g1
	wr %g1, %g0, %y

	ld [%sp + 0x04], %g1
	andn %g1, (PSR_CWP | PSR_ET), %g1
	or %g1, PSR_PIL, %g1
	wr %g1, %psr
	nop
	nop
	nop

	ld [%sp + 0x88], %g7
	ld [%sp + 0x84], %g6
	ld [%sp + 0x7c], %g4
	ld [%sp + 0x78], %g3
	ld [%sp + 0x74], %g2

	ldd [%sp + 0x68], %i6
	ldd [%sp + 0x60], %i4
	ldd [%sp + 0x58], %i2
	ldd [%sp + 0x50], %i0

	ldd [%sp + 0x48], %l6
	ldd [%sp + 0x40], %l4
	ldd [%sp + 0x38], %l2
	ldd [%sp + 0x30], %l0

	ld  [%sp + 0x2c], %o7
	ldd [%sp + 0x20], %o4
	ldd [%sp + 0x18], %o2
	ldd [%sp + 0x10], %o0

	/* enable traps */
	rd %psr, %g1
	or %g1, PSR_ET, %g1
	wr %g1, %psr
	nop
	nop
	nop
	
	ld [%sp + 0x70], %g1

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
	ld [%o0], %sp
	ld [%sp + 0x8c], %g1
	wr %g1, %g0, %y

	ld [%sp + 0x88], %g7
	ld [%sp + 0x84], %g6
	ld [%sp + 0x80], %g5
	ld [%sp + 0x7c], %g4
	ld [%sp + 0x78], %g3
	ld [%sp + 0x74], %g2
	ld [%sp + 0x70], %g1

	ldd [%sp + 0x68], %i6
	ldd [%sp + 0x60], %i4
	ldd [%sp + 0x58], %i2
	ldd [%sp + 0x50], %i0

	ldd [%sp + 0x48], %l6
	ldd [%sp + 0x40], %l4
	ldd [%sp + 0x38], %l2
	ldd [%sp + 0x30], %l0

	ld  [%sp + 0x2c], %o7
	ldd [%sp + 0x20], %o4
	ldd [%sp + 0x18], %o2
	ldd [%sp + 0x10], %o0

	/* load return address */
	ld [%sp + 0x08], %l6

	/* enable interrupts */
	ta  0x0a
	add %sp, 0x90, %sp

	jmp %l6
	ld  [%sp - 0x48], %l6
.size hal_longjmp, . - hal_longjmp
