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

.extern hal_cpuKernelStack

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
	TRAP(_traps_flushWindows)  /* 0x83 - flush windows */
	BAD_TRAP                   /* 0x84 - clean windows */
	BAD_TRAP                   /* 0x85 - range check */
	BAD_TRAP                   /* 0x86 - fix alignment */
	BAD_TRAP                   /* 0x87 - integer overflow */
	BAD_TRAP                   /* 0x88 - syscall */
	/* Reserved for OS */
	TRAP(_interrupts_disable)  /* 0x89 - disable interrupts (reserved for OS) */
	TRAP(_interrupts_enable)   /* 0x8a - enable interrupts (reserved for OS) */
	BAD_TRAP

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
	ldd [%sp + 0x38], %fp
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
	/* move to next window and save it on stack */
	save
	std %l0, [%sp + 0x00]
	std %l2, [%sp + 0x08]
	std %l4, [%sp + 0x10]
	std %l6, [%sp + 0x18]
	std %i0, [%sp + 0x20]
	std %i2, [%sp + 0x28]
	std %i4, [%sp + 0x30]
	std %fp, [%sp + 0x38]
	/* calculate new wim */
	mov %wim, %l3
	sll	%l3, (NWINDOWS - 1), %l4 /* %l4 = wim << (NWINDOWS - 1) */
	srl %l3, 1, %l3              /* %l3 = wim >> 1 */
	wr %l3, %l4, %wim            /* %wim = %l3 ^ %l4 */
	nop
	nop
	nop
	restore /* go back to window that caused trap */
	jmp %l1 /* re-execute save that caused trap */
	rett %l2

.size _traps_winOverflowHandler, . - _traps_winOverflowHandler


/* Flush windows handler.
 * This handler flushes all used windows to stack
 * (similar to context switching),
 * except current and the one we'll return to.
 * on entry:
 * %psr in %l0, PC in %l1, nPC in %l2
 */

.global _traps_flushWindows
.type _traps_flushWindows, #function

_traps_flushWindows:
	/* save used global registers */
	mov %g1, %l3
	mov %g2, %l4
	mov %g3, %l5
	mov %g4, %l6
	mov %g5, %l7

	and %l0, PSR_CWP, %g3

	/* Current state of registers:
	 * %g3 = CWP
	 * %g4 = %wim
	 */

	mov %g3, %g2 /* save CWP in %g2 */

	/* set bit in register %g3, which corresponds to CWP
	 * %g3 = 1 << %g3 (CWP)
	 */

	mov 1, %g4
	sll %g4, %g3, %g3

	mov %l0, %g5 /* save %psr in %g5 */

	sethi %hi(_interrupts_save_context), %l0
	jmpl %l0 + %lo(_interrupts_save_context), %g1 /* clobbers %g1, %g3 */
	rd %wim, %g4

flush_win_done:
	/* calculate new wim for CWP + 2
	 * currently %g2 = CWP
	 */
	add %g2, 2, %g2
	cmp %g2, NWINDOWS
	bl fw_cwp_done
	nop

	be fw_cwp_done
	mov 0, %g2
	mov 1, %g2

fw_cwp_done:
	/* %g2 = CWP + 2 (mod NWINDOWS) */
	mov 1, %g3
	sll %g3, %g2, %g3
	wr %g3, %wim
	nop
	nop
	nop

	/* restore %psr */
	mov %g5, %psr
	nop
	nop
	nop

	/* restore used global registers */
	mov %l3, %g1
	mov %l4, %g2
	mov %l5, %g3
	mov %l6, %g4
	mov %l7, %g5

	jmp %l2
	rett %l2 + 4
.size _traps_flushWindows, . - _traps_flushWindows


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
	std %fp, [%sp + 0x78]

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
 */

.global _traps_syscall
.type _traps_syscall, #function
_traps_syscall:
	/* Syscall trap is called from assembly code,
	 * so we know which registers are used.
	 * Thus we won't save global registers
	 * (none are important for the caller).
	 * This does not conform to the ABI.
	 */

	mov %wim, %g2
	/* check if we've just overflowed
	 * window overflow if wim == (1 << CWP)
	 * wim >> l0[4:0] - shift wim by CWP (lowest 5 bits from psr)
	 */
	srl %g2, %l0, %g3
	cmp %g3, 1

	bne s_wovfl_done
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

s_wovfl_done:
	sub %fp, 0x18, %sp
	/* write arguments to stack */
	std %i0, [%sp]
	std %i2, [%sp + 0x8]
	std %i4, [%sp + 0x10]

	mov %sp, %o1 /* ustack */

	/* swap to kernel stack */
	sethi %hi(hal_cpuKernelStack), %l5
	ld [%l5 + %lo(hal_cpuKernelStack)], %sp

	/* allocate stack frame for syscall handler */
	sub %sp, 0x60, %sp
	mov %g4, %o0 /* syscall number */

	/* enable traps, interrupts */
	andn %l0, (PSR_PIL), %l1
	wr %l1, (PSR_ET), %psr
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

	/* Check if restore would cause underflow.
	 * After restore: CWP = CWP + 1
	 * i.e. wim >> (CWP + 1) == 1
	 */
	rd %psr, %g3
	and %g3, PSR_CWP, %g3
	/* save CWP */
	mov %g3, %g5
	add %g3, 1, %g3
	cmp %g3, NWINDOWS
	bne s_cwp_done
	nop
	mov 0, %g3

s_cwp_done:
	rd %wim, %g4
	srl %g4, %g3, %g3
	cmp %g3, 1
	/* if wim >> (CWP + 1) != 1, it's ok */
	bne s_wunfl_done
	/* uses the delay slot
	 * calculate new wim
	 * %g4 = current %wim
	 * wim = (wim << 1) ^ (wim >> (NWINDOWS - 1))
	 */
	sll %g4, 1, %l5
	srl %g4, (NWINDOWS - 1), %g4
	wr %g4, %l5, %wim
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

s_wunfl_done:
	mov %o0, %i0 /* pass return value to caller */

	/* swap window if needed (after vfork) */
	andn %l0, PSR_CWP, %l0
	or %l0, %g5, %l0
	andn %l0, (PSR_PS | PSR_ET), %l0
	wr %l0, %psr
	nop
	nop
	nop

	jmp %l2
	rett %l2 + 4
.size _traps_syscall, . - _traps_syscall
