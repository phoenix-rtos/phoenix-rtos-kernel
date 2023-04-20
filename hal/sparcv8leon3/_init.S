/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low level initialization
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include "config.h"
#include <arch/cpu.h>

.extern syspage

.section	".text"
.align		4
.global		_init
_init:
	/* Set up trap table */
	sethi %hi(_trap_table), %g1
	wr %g1, %tbr

	wr %g0, 0x2, %wim

	/* Point to syspage */
	sethi %hi(syspage), %g1
	st %g2, [%g1 + %lo(syspage)]

	/* Set PSR to "supervisor", enable traps, disable interrupts, set CWP to 0 */
	mov %psr, %g1
	or %g1, (PSR_ET | PSR_S | PSR_PIL), %g1
	andn %g1, (PSR_CWP), %g1
	wr %g1, %psr
	nop
	nop
	nop

	/* Set stack pointer */
	clr %fp
	sethi %hi(_init_stack), %g1
	ld [%g1 + %lo(_init_stack)], %sp
	sub %sp, 0x60, %sp

	call main
	mov %g0, %g1
.size _init, . - _init

.align		8
.global		_init_stack
_init_stack:
	.word _end + SIZE_KSTACK
