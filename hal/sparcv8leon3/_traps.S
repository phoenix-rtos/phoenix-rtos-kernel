/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Trap table for sparcv8leon3
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

#define TBR_TT_MSK 0xFF0
#define TBR_TT_SHIFT 4

#define BAD_TRAP \
	mov %psr, %l0; \
	sethi %hi(_exceptions_dispatch), %l3; \
	jmp %l3 + %lo(_exceptions_dispatch); \
	nop;

#define TRAP(handler) \
	mov %psr, %l0; \
	sethi %hi(handler),%l3; \
	jmp %l3 + %lo(handler); \
	nop;

#define INTERRUPT(irqn) \
	mov %psr, %l0; \
	sethi %hi(_interrupts_dispatch),%l3; \
	jmp %l3 + %lo(_interrupts_dispatch); \
	mov (irqn), %l3;

#define RESET \
	mov	%g0, %g4; \
	sethi %hi(_init), %g4; \
	jmp %g4 + %lo(_init); \
	nop;

/* When trap occurs, traps are disabled: ET <- 0
 * existing supervisor mode is saved in PS <- S
 * supervisor mode is enabled: S <- 1
 * register window is advanced to the next window: CWP <- (CWP - 1) % 31
 * without test for window overflow
 * PSR -> %l0, PC -> %l1, nPC -> %l2
 * tt field is written to the particular value that identifies the trap
 */

/* Trap table consists of 256 4-instruction entries */
.section	".init", "ax"
.global	_trap_table
.global	_start

_trap_table:
	/* 0x00 - Hardware traps */
.align 0x1000
.type	_start, #function
_start:
	RESET                     /* 0x00 - reset */
.size _start, . - _start
	BAD_TRAP                         /* 0x01 - instruction access exception */
	BAD_TRAP                         /* 0x02 - illegal instruction */
	BAD_TRAP                         /* 0x03 - privileged instruction */
	BAD_TRAP                         /* 0x04 - fp disabled */
	TRAP(_traps_winOverflowHandler)  /* 0x05 - window overflow */
	TRAP(_traps_winUnderflowHandler) /* 0x06 - window underflow */
	BAD_TRAP                         /* 0x07 - mem address not aligned */
	BAD_TRAP                         /* 0x08 - fp exception */
	BAD_TRAP                         /* 0x09 - data access exception */
	BAD_TRAP                         /* 0x0a - tag overflow */
	BAD_TRAP                         /* 0x0b - watchpoint detected */
	BAD_TRAP                         /* 0x0c - reserved */
	BAD_TRAP                         /* 0x0d - reserved */
	BAD_TRAP                         /* 0x0e - reserved */
	BAD_TRAP                         /* 0x0f - reserved */
	BAD_TRAP                         /* 0x10 - reserved */

	/* 0x11 - Interrupts */

	INTERRUPT(1)              /* 0x11 - interrupt level 1 */
	INTERRUPT(2)              /* 0x12 - interrupt level 2 */
	INTERRUPT(3)              /* 0x13 - interrupt level 3 */
	INTERRUPT(4)              /* 0x14 - interrupt level 4 */
	INTERRUPT(5)              /* 0x15 - interrupt level 5 */
	INTERRUPT(6)              /* 0x16 - interrupt level 6 */
	INTERRUPT(7)              /* 0x17 - interrupt level 7 */
	INTERRUPT(8)              /* 0x18 - interrupt level 8 */
	INTERRUPT(9)              /* 0x19 - interrupt level 9 */
	INTERRUPT(10)             /* 0x1a - interrupt level 10 */
	INTERRUPT(11)             /* 0x1b - interrupt level 11 */
	INTERRUPT(12)             /* 0x1c - interrupt level 12 */
	INTERRUPT(13)             /* 0x1d - interrupt level 13 */
	INTERRUPT(14)             /* 0x1e - interrupt level 14 */
	INTERRUPT(15)             /* 0x1f - interrupt level 15 */

	/* 0x20 - GR716 - defined traps */

	BAD_TRAP                  /* 0x20 - r register access error */
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP                  /* 0x2a - division by zero */
	BAD_TRAP                  /* 0x2b - data store error */
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP
	BAD_TRAP

	/* 0x30 - 0x7F - reserved */

	.rept 80
	BAD_TRAP
	.endr

	/* 0x80 - ABI defined traps */

	TRAP(_traps_syscall)       /* 0x80 - syscall */
	BAD_TRAP                   /* 0x81 - breakpoint */
	BAD_TRAP                   /* 0x82 - division by zero */
	BAD_TRAP                   /* 0x83 - flush windows */
	BAD_TRAP                   /* 0x84 - clean windows */
	BAD_TRAP                   /* 0x85 - range check */
	BAD_TRAP                   /* 0x86 - fix alignment */
	BAD_TRAP                   /* 0x87 - integer overflow */
	BAD_TRAP                   /* 0x88 - syscall */
	/* Reserved for OS */
	TRAP(_interrupts_disable)  /* 0x89 - disable interrupts (reserved for OS) */
	TRAP(_interrupts_enable)   /* 0x8a - enable interrupts (reserved for OS) */
	TRAP(_traps_rescheduleRet) /* 0x8b - return from reschedule (reserved for OS) */

	/* 0x8c - 0xFF - reserved for OS */

	.rept 116
	BAD_TRAP
	.endr


/* Trap handlers */

.section ".text"
.align 4


/* Window underflow handler
* on entry:
* %psr in %l0, PC in %l1, nPC in %l2
*/

.global _traps_winUnderflowHandler
.type _traps_winUnderflowHandler, #function

_traps_winUnderflowHandler:
	/* calculate new wim
	 * wim = (wim << 1) ^ (wim >> (NWINDOWS - 1))
	 */
	mov %wim, %l3
	sll %l3, 1, %l4
	srl %l3, (NWINDOWS - 1), %l3
	wr %l3, %l4, %wim
	nop
	nop
	nop
	/* go back to window that caused the trap */
	restore
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
	save
	jmp %l1
	rett %l2

.size _traps_winUnderflowHandler, . - _traps_winUnderflowHandler

/* Window overflow handler
 * on entry:
 * %psr in %l0, PC in %l1, nPC in %l2
 */

.global _traps_winOverflowHandler
.type _traps_winOverflowHandler, #function

_traps_winOverflowHandler:
	/* move to next window and save it on stack (using d-word store) */
	save
	std %l0, [%sp + 0x00]
	std %l2, [%sp + 0x08]
	std %l4, [%sp + 0x10]
	std %l6, [%sp + 0x18]
	std %i0, [%sp + 0x20]
	std %i2, [%sp + 0x28]
	std %i4, [%sp + 0x30]
	std %i6, [%sp + 0x38]
	/* calculate new wim */
	mov %wim, %l3
	sll	%l3, (NWINDOWS - 1), %l4 /* %l3 = wim << (NWINDOWS - 1) */
	srl %l3, 1, %l3              /* %l3 = wim >> 1 */
	wr %l3, %l4, %wim            /* %wim = %l3 ^ %l4 */
	nop
	nop
	nop
	restore /* go back to window that caused trap */
	jmp %l1 /* re-execute save that caused trap */
	rett %l2

.size _traps_winOverflowHandler, . - _traps_winOverflowHandler


/* BAD_TRAP handler
 * on entry:
 * %psr in %l0, PC in %l1, nPC in %l2
 * must not touch global or out registers until saved on stack
 */

.global _exceptions_dispatch
.type _exceptions_dispatch, #function
_exceptions_dispatch:
	/* go back to window that caused the trap */
	restore
	/* 152 bytes needed for exception context */
	sub %sp, 0x98, %sp

	/* save context on stack */
	std %g0, [%sp + 0x00]
	std %g2, [%sp + 0x08]
	std %g4, [%sp + 0x10]
	std %g6, [%sp + 0x18]

	std %o0, [%sp + 0x20]
	std %o2, [%sp + 0x28]
	std %o4, [%sp + 0x30]
	/* sp stored later */
	st %o7, [%sp + 0x3c]

	std %l0, [%sp + 0x40]
	std %l2, [%sp + 0x48]
	std %l4, [%sp + 0x50]
	std %l6, [%sp + 0x58]

	std %i0, [%sp + 0x60]
	std %i2, [%sp + 0x68]
	std %i4, [%sp + 0x70]
	std %i6, [%sp + 0x78]

	/* save sp */
	add %sp, 0x98, %l0
	st %l0, [%sp + 0x38]

	rd  %y, %g1
	st %g1, [%sp + 0x80]
	rd  %psr, %g1
	xor %g1, PSR_ET, %g1
	st %g1, [%sp + 0x84]
	rd %wim, %g1
	st %g1, [%sp + 0x88]
	rd %tbr, %g1
	st %g1, [%sp + 0x8c]

	save

	/* store pc, npc */
	st %l1, [%fp + 0x90]
	st %l2, [%fp + 0x94]

	/* allocate stack frame for exception handler */
	sub %fp, 0x60, %sp

	/* enable traps, disable interrupts */
	or %l0, PSR_PIL, %l4
	wr %l4, PSR_ET, %psr
	nop
	nop
	nop

	/* trap type */
	and %g1, TBR_TT, %o0
	srl %o0, TBR_TT_SHIFT, %o0

	/* void exceptions_dispatch(unsigned int n, exc_context_t *ctx) */
	call exceptions_dispatch
	/* ctx */
	mov %fp, %o1
.size _exceptions_dispatch, . - _exceptions_dispatch

/* syscall dispatcher
 * on entry:
 * %psr in %l0, PC in %l1, nPC in %l2
 * syscall number in %g4
 * carry bit set indicates error
 */

.global _traps_syscall
.type _traps_syscall, #function
_traps_syscall:
	mov %l0, %g1
	mov %l2, %g2

	/* handle everything in the same window */
	restore

	std %l0, [%sp + 0x00]
	st  %i7, [%sp + 0x3c]

	sub %sp, 0x70, %sp

	/* write arguments to stack */
	st %o0, [%sp + 0x44]
	st %o1, [%sp + 0x48]
	st %o2, [%sp + 0x4c]
	st %o3, [%sp + 0x50]
	st %o4, [%sp + 0x54]
	st %o5, [%sp + 0x58]

	st %g1, [%sp + 0x68] /* psr */
	st %g2, [%sp + 0x6c] /* nPC */

	mov %g4, %o0 /* syscall number */
	add %sp, 0x44, %o1 /* ustack */

	rd %psr, %l0
	andn %l0, (PSR_PIL), %l0
	wr %l0, PSR_ET, %psr
	nop
	nop
	nop

	/* void *syscalls_dispatch(int n, char *ustack) */
	call syscalls_dispatch
	nop

	/* disable traps */
	pwr 0, %psr
	nop
	nop
	nop

	ld  [%sp + 0x68], %g1 /* psr */
	ld  [%sp + 0x6c], %g2 /* nPC */

	add %sp, 0x70, %sp

	ldd [%sp + 0x00], %l0
	ld  [%sp + 0x3c], %i7

	save

	wr %g1, %psr
	nop
	nop
	nop

	jmp %g2
	rett %g2 + 4
.size _traps_syscall, . - _traps_syscall

/* hal_cpuReschedule return trap handler.
 *
 * Returning through this trap handler allows us
 * to set both PC and nPC (using the jmp-rett pair).
 * Setting nPC is necessary if execution was interrupted
 * in the delay slot of a branch instruction.
 *
 * On entry: %psr in %l0
 * PC at [%fp + 0x08]
 * nPC at [%fp + 0x0c]
 */

.global _traps_rescheduleRet
.type _traps_rescheduleRet, #function
_traps_rescheduleRet:
	ld [%fp + 0x08], %l1 /* PC */
	ld [%fp + 0x0c], %l2 /* nPC */

	add %fp, 0x90, %fp

	/* enable interrupts */
	andn %l0, PSR_PIL, %l0
	wr %l0, %psr
	nop
	nop
	nop

	jmp %l1
	rett %l2
.size _traps_rescheduleRet, . - _traps_rescheduleRet
