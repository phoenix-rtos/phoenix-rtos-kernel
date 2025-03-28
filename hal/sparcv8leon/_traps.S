/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Trap table for sparcv8leon
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

#include "gaisler/grlib-tn-0018.h"

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
	/* PC-relative, no need to worry about MMU */ \
	call _init; \
	nop; \
	nop; \
	nop;

/* When trap occurs, traps are disabled: ET <- 0
 * existing supervisor mode is saved in PS <- S
 * supervisor mode is enabled: S <- 1
 * register window is advanced to the next window: CWP <- (CWP - 1) % NWINDOWS,
 * without test for window overflow;
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
	TRAP(_traps_fpDisabledHandler)   /* 0x04 - fp disabled */
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
#ifdef __CPU_GR716
	BAD_TRAP
#else
	TRAP(_traps_stErrHandler) /* 0x2b - data store error */
#endif
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
	TRAP(_traps_disable)       /* 0x8b - disable traps - where pwr not supported (reserved for OS) */
	TRAP(_traps_halJmpRet)     /* 0x8c - safe hal_jmp return (reserved for OS) */

	/* 0x8d - 0xFF - reserved for OS */

	.rept 116
	BAD_TRAP
	.endr


/**************** Trap handlers ****************/

.section ".text"
.align 4

#ifndef __CPU_GR716

/* Data store error handler */

.type _traps_stErrHandler, #function
_traps_stErrHandler:
	sta %g0, [%g0] ASI_CACHE_CTRL

	TN_0018_WAIT_ICACHE(%l3, %l4)

	/* restore %psr */
	wr %l0, %psr
	nop
	nop
	nop

	TN_0018_FIX(%l3, %l4)

	jmp %l1
	rett %l2
.size _traps_stErrHandler, . - _traps_stErrHandler

#endif

/* FPU disabled handler (enables FPU) */

.type _traps_fpDisabledHandler, #function
_traps_fpDisabledHandler:
/* NOTE: QEMU does not have FPU info in the asr17, assume the FPU is present. */
#ifndef __CPU_GENERIC
	/* check if FPU present */
	rd %asr17, %l3
	srl %l3, 10, %l3
	andcc %l3, 0x3, %g0
	bz _exceptions_dispatch
	nop
#endif
	/* enable FPU */
	set PSR_EF, %l3
	or %l0, %l3, %l0
	wr %l0, %psr
	nop
	nop
	nop

	jmp %l1
	rett %l2
.size _traps_fpDisabledHandler, . - _traps_fpDisabledHandler


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
	mov %l2, %l1 /* save %nPC in %l1 */

	sethi %hi(_interrupts_saveContext), %l0
	jmpl %l0 + %lo(_interrupts_saveContext), %g1 /* clobbers %g1, %g3, %l2 */
	rd %wim, %g4

flush_win_done:
	/* calculate new wim for CWP + 2
	 * currently %g2 = CWP
	 */
	add %g2, 2, %g2
	cmp %g2, NWINDOWS
	bl fw_cwp_done
	mov 1, %g3

	be fw_cwp_done
	mov 0, %g2
	mov 1, %g2

fw_cwp_done:
	/* %g2 = CWP + 2 (mod NWINDOWS) */
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

	jmp %l1
	rett %l1 + 4
.size _traps_flushWindows, . - _traps_flushWindows


/* BAD_TRAP handler
 * on entry:
 * %psr in %l0, PC in %l1, nPC in %l2
 * must not touch global or out registers until saved on stack
 */

.global _exceptions_dispatch
.type _exceptions_dispatch, #function
_exceptions_dispatch:
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

	bne exc_wovfl_done
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

exc_wovfl_done:
	/* check if we need to swap to kernel stack
	 * i.e. when PSR_PS is not set
	 */
	andcc %l0, PSR_PS, %g0
	bnz exc_no_kstack_switch

	set hal_cpuKernelStack, %l6
	/* Extract CPU ID and add offset */
	rd %asr17, %l7
	srl %l7, 28, %l7
	sll %l7, 2, %l7
	ld [%l6 + %l7], %l7
	ba exc_kstack_set
	sub %l7, CPU_EXC_SIZE, %sp

exc_no_kstack_switch:
	/* we came from kernel, make space for context */
	sub %fp, CPU_EXC_SIZE, %sp

exc_kstack_set:
	/* Save context on kernel stack */
	set PSR_EF, %g2
	andcc %l0, %g2, %g0
	bz exc_fpu_done
	nop

	FPU_SAVE

exc_fpu_done:

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

	rd %wim, %g2
	rd %tbr, %g3
	std %g2, [%sp + 0xd8] /* wim, tbr */

	mov %sp, %l7
	sub %sp, 0x60, %sp

	mov %l7, %o1 /* (exc_context_t *) */

	mov %l0, %g2
	mov 1, %g4
	sll %g4, %g2, %g3 /* %g3 = 1 << %g2[4:0] (CWP) */

	sethi %hi(_interrupts_saveContext), %g5
	jmpl %g5 + %lo(_interrupts_saveContext), %g1 /* clobbers %g1, %g3, %l2 */
	rd %wim, %g4

	/* enable traps, disable interrupts, restore CWP */
	or %g2, (PSR_PIL | PSR_ET), %g2
	wr %g2, %psr
	nop
	nop
	nop

	/* trap type */
	ld [%l7 + 0xdc], %o0
	srl %o0, TBR_TT_SHIFT, %o0
	/* void exceptions_dispatch(unsigned int n, exc_context_t *ctx) */
	call exceptions_dispatch
	and %o0, 0xff, %o0

	/* disable traps */
#ifdef LEON_USE_PWR
	pwr 0, %psr
	nop
	nop
	nop
#else
	ta 0xb
#endif
	mov %l7, %sp
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

	set PSR_EF, %l5
	andcc %l0, %l5, %g0
	bz exc_restore_fpu_done
	nop

	FPU_RESTORE

exc_restore_fpu_done:
	/* Check if restore would cause window underflow.
	 * After restore: CWP = CWP + 1 (mod NWINDOWS)
	 * i.e. wim >> (CWP + 1) == 1
	 */

	and	%l0, PSR_CWP, %l5
	add %l5, 1, %l5
	cmp %l5, NWINDOWS
	bne exc_cwp_done1
	rd %wim, %l4

	/* we'd end up in non-existent window #NWINDOWS, it means it's #0 */
	mov 0, %l5

exc_cwp_done1:
	/* l4 = wim, l5 = CWP + 1 (mod NWINDOWS)
	 * check if wim >> (CWP + 1) == 1 (window underflow)
	 */
	srl %l4, %l5, %l6
	cmp %l6, 1
	bne exc_return
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

exc_return:
	TN_0018_WAIT_ICACHE(%l3, %l4)

	wr %l0, %psr
	nop
	nop
	nop

	TN_0018_FIX(%l3, %l4)

	jmp %l1
	rett %l2
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
	/* for signal handling: */
	restore

	std %l0, [%sp + 0x00]
	std %l2, [%sp + 0x08]
	std %l4, [%sp + 0x10]
	std %l6, [%sp + 0x18]
	std %i0, [%sp + 0x20]
	std %i2, [%sp + 0x28]
	std %i4, [%sp + 0x30]
	std %fp, [%sp + 0x38]

	save

	/* write arguments to stack reserved space */
	st %i0, [%fp + 0x44]
	st %i1, [%fp + 0x48]
	st %i2, [%fp + 0x4c]
	st %i3, [%fp + 0x50]
	st %i4, [%fp + 0x54]
	st %i5, [%fp + 0x58]

	add %fp, 0x44, %o1 /* ustack */

	/* swap to kernel stack */
	set hal_cpuKernelStack, %l5
	/* Extract CPU ID and add offset */
	rd %asr17, %sp
	srl %sp, 28, %sp
	sll %sp, 2, %sp
	ld [%l5 + %sp], %sp

	/* for signal handling: */
	sub %sp, CPU_CTX_SIZE, %sp
	st  %l0, [%sp + 0x08] /* psr */
	add %l2, 4, %l3
	st  %l2, [%sp + 0x0c] /* pc */
	st  %l3, [%sp + 0x10] /* npc */
	std %g4, [%sp + 0x20]
	std %g6, [%sp + 0x28]

	set PSR_EF, %g2
	andcc %l0, %g2, %g0
	bz s_fpu_done
	std %fp, [%sp + 0x48] /* fp (task's sp), return address */

	FPU_SAVE

s_fpu_done:
	/* allocate stack frame for syscall handler */
	sub %sp, 0x60, %sp
	mov %g4, %o0 /* syscall number */

	/* enable traps, interrupts */
	andn %l0, (PSR_PIL), %l1
	wr %l1, (PSR_ET), %psr
	nop
	nop
	nop

	/* void *syscalls_dispatch(int n, char *ustack, cpu_context_t *ctx) */
	call syscalls_dispatch
	add %sp, 0x60, %o2

	/* disable traps */
#ifdef LEON_USE_PWR
	pwr 0, %psr
	nop
	nop
	nop
#else
	ta 0xb
#endif

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
	rd %wim, %g4

	mov 0, %g3

s_cwp_done:
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
	andn %l0, (PSR_CWP | PSR_PS | PSR_ET), %l0
	or %l0, %g5, %l0
	wr %l0, %psr
	nop
	nop
	nop

	jmp %l2
	rett %l2 + 4
.size _traps_syscall, . - _traps_syscall


/* Disable traps
 * Must be called from supervisor mode
 * Next instruction after `ta` instruction
 * must not be a CTI - nPC not written.
 */
.global _traps_disable
.type _traps_disable, #function
_traps_disable:
	jmp %l2
	restore
.size _traps_disable, . - _traps_disable


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



/* Safe userspace hal_jmp return
 * %l0 - psr
 * %g1 - return address
 */
.global _traps_halJmpRet
.type _traps_halJmpRet, #function
_traps_halJmpRet:
	andn %l0, (PSR_PIL | PSR_PS), %l0
	wr %l0, %psr
	nop
	nop
	nop
	jmp %g1
	rett %g1 + 4
.size _traps_halJmpRet, . - _traps_halJmpRet


/*************** Helper functions ***************/


/* This function saves all valid windows on stack.
 * Before calling, following registers must be set:
 * %g1 - call address
 * %g3 - bit set for CWP
 * %g4 - wim
 *
 * Clobbers %g1, %g3, %l2
 */
.global _interrupts_saveContext
.type _interrupts_saveContext, #function
_interrupts_saveContext:

save_context_loop:
	sll %g3, 1, %l2
	srl %g3, (NWINDOWS - 1), %g3
	or %g3, %l2, %g3
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

	ba save_context_loop
	std %fp, [%sp + 0x38]

save_context_done:
	jmpl %g1 + 8, %g0
	nop
.size _interrupts_saveContext, . - _interrupts_saveContext


/* This function restores current window (except %g1, %g2, %g3)
 * and first task's window.
 * Before calling, following registers must be set:
 * %g1 - call address
 * %g2 - pointer to context
 *
 * On return: task's %psr in %g2
 */
.global _interrupts_restoreContext
.type _interrupts_restoreContext, #function
_interrupts_restoreContext:
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

	set PSR_EF, %g2
	andcc %l0, %g2, %g0
	bz restore_fpu_done
	nop

	FPU_RESTORE

restore_fpu_done:
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

	jmpl %g1 + 8, %g0
	/* go back to handler's window */
	save
.size _interrupts_restoreContext, . - _interrupts_restoreContext


/* int hal_cpuReschedule(struct _spinlock_t *spinlock, spinlock_ctx_t *scp) */
.global	hal_cpuReschedule
.type	hal_cpuReschedule, #function
hal_cpuReschedule:
	save %sp, -CPU_CTX_SIZE, %sp

#ifdef LEON_USE_PWR
	pwr 0, %psr
	nop
	nop
	nop
#else
	ta 0xb
#endif

	/* this is voluntary reschedule,
	 * no need to store caller-saves registers
	 * (outputs of previous window and g1-g5)
	 */
	rd %psr, %l0
	or %l0, PSR_PS, %l0

	set PSR_EF, %g2
	andcc %l0, %g2, %g0
	bz r_fpu_done
	nop

	FPU_SAVE

r_fpu_done:
	mov %sp, %l1    /* save pointer to context */

	st  %sp, [%sp + 0x00]
	st  %g0, [%sp + 0x04]
	st  %l0, [%sp + 0x08] /* psr */
	add %i7, 0x8, %g3
	st  %g3, [%sp + 0x0c] /* pc */
	add %g3, 0x4, %g3
	st  %g3, [%sp + 0x10] /* npc */
	std %g6, [%sp + 0x28]
	st  %g0, [%sp + 0x30] /* default return value */
	st  %fp, [%sp + 0x48]

	sub %sp, 0x60, %sp

	/* enable traps, keep interrupts disabled */
	or %l0, (PSR_PIL | PSR_ET), %l0
	wr %l0, %psr
	nop
	nop
	nop

	mov %l0, %g2

	mov 1, %g4
	sll %g4, %g2, %g3 /* %g3 = 1 << %g2[4:0] (CWP) */

	sethi %hi(_interrupts_saveContext), %g5
	jmpl %g5 + %lo(_interrupts_saveContext), %g1 /* clobbers %g1, %g3, %l2 */
	rd %wim, %g4

	wr %g2, %psr
	nop
	nop
	nop

	and %g2, PSR_CWP, %g2
	add %g2, 1, %g2
	cmp %g2, NWINDOWS
	bne r_cwp_done
	mov 1, %g3

	mov 0, %g2

r_cwp_done:
	/* set %wim to 1 << %g2 (CWP + 1) */
	sll %g3, %g2, %g2
	wr %g2, %wim

	/* check if spinlock is not NULL */
	cmp %i0, %g0
	beq r_no_spinlock
	nop

r_spinlock:
	clr %o0
	mov %l1, %o1 /* cpu_context_t * */
	call _threads_schedule
	clr %o2

	/* clear spinlock */
	stbar
	ba r_return
	stub %g0, [%i0 + 0x0c]

r_no_spinlock:

	clr %o0
	mov %l1, %o1 /* cpu_context_t * */
	call threads_schedule
	clr %o2

r_return:
	/* disable traps */
#ifdef LEON_USE_PWR
	pwr 0, %psr
	nop
	nop
	nop
#else
	ta 0xb
#endif

	MULTILOCK_CLEAR

	ld [%l1], %g2 /* ctx pointer */

	/* Set %psr of the new task.
	 * This will cause window to be switched
	 * so that the new task's window is CWP + 1.
	 */

	mov %g0, %wim /* we don't need it now */
	ld [%g2 + 0x08], %g1
	nop
	andn %g1, PSR_ET, %g1 /* disable traps */
	wr %g1, %psr

	sethi %hi(_interrupts_restoreContext), %g5
	jmpl %g5 + %lo(_interrupts_restoreContext), %g1
	nop

	/* check CWP overflow (same as before) */
	and %g2, PSR_CWP, %g2
	add %g2, 1, %g2
	cmp %g2, NWINDOWS
	bne r_cwp_done1
	mov 1, %g3

	mov 0, %g2

r_cwp_done1:
	/* set %wim to 1 << %g2 (CWP + 1) */
	sll %g3, %g2, %g2
	wr %g2, %wim
	nop

	/* restore %g1, %g2, %g3 */
	ld  [%sp + 0x14], %g1
	ldd [%sp + 0x18], %g2

	TN_0018_WAIT_ICACHE(%l3, %l4)

	andn %l0, PSR_ET, %l0
	wr %l0, %psr
	nop
	nop
	nop

	TN_0018_FIX(%l3, %l4)

	/* Set both PC and nPC */
	jmp %l1
	rett %l2
.size hal_cpuReschedule, . - hal_cpuReschedule


/**************** Jump functions ****************/


/* void hal_jmp(void *f, void *kstack, void *ustack, int kargc, const arg_t *kargs) */
.global	hal_jmp
.type	hal_jmp, #function
hal_jmp:
	save %sp, -0x60, %sp
#ifdef LEON_USE_PWR
	pwr 0, %psr
	nop
	nop
	nop
#else
	ta 0xb
#endif

	/* calculate new wim */
	rd %psr, %l0
	and %l0, PSR_CWP, %l1
	add %l1, 1, %l1
	cmp %l1, NWINDOWS
	bne 1f
	mov 1, %l2

	mov 0, %l1
1:
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
	ld [%i4], %o0
	subcc %i3, 1, %i3
	bneg 2f
	ld [%i4 + 4], %o1
	subcc %i3, 1, %i3
	bneg 2f
	ld [%i4 + 8], %o2
	ld [%i4 + 12], %o3
2:
	sub %fp, 0x60, %sp
	wr %l0, PSR_ET, %psr
	nop
	nop
	nop
	call %i0
	nop
3:
	mov %i2, %fp
	sub %fp, 0x60, %sp

	/* Enable traps, disable interrupts */
	or %l0, (PSR_ET | PSR_PIL), %l0
	wr %l0, %psr
	nop
	nop
	nop
	mov %i0, %g1
	/* safely jump to userspace through trap */
	ta 0xc
.size hal_jmp, . - hal_jmp
