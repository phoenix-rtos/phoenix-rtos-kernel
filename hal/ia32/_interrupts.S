/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt stubs
 *
 * Copyright 2012, 2016, 2020, 2023 Phoenix Systems
 * Copyright 2001, 2005 Pawel Pisarczyk
 * Author; Pawel Pisarczyk, Jan Sikorski, Gerard Swiderski, Andrzej Stalke
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>
#include <arch/interrupts.h>

.extern _interrupts_multilock

.text


#define CTXPUSHL 38 /* 10 + 28 (FPU) */

#define EAX_OFFSET (16 + (FPU_CONTEXT_SIZE))


.macro _INTERRUPTS_MULTILOCKSET reg
	xorl \reg, \reg
1:
	xchgl _interrupts_multilock, \reg
	orl \reg, \reg
	jz 1b
.endm


.macro _INTERRUPTS_MULTILOCKCLEAR reg
	xorl \reg, \reg
	incl \reg
	xchgl _interrupts_multilock, \reg
.endm


.global hal_lockScheduler
.align 4, 0x90
hal_lockScheduler:
	call hal_tlbShootdown
	_INTERRUPTS_MULTILOCKSET %eax
	ret
.size hal_lockScheduler, .-hal_lockScheduler


.global interrupts_pushContext
.align 4, 0x90
interrupts_pushContext:
	xchgl (%esp), %edx
	movl %edx, -(4 * CTXPUSHL)(%esp)
	popl %edx
	/* Check TS flag in CR0 register */
	pushl %eax
	movl %cr0, %eax
	subl $FPU_CONTEXT_SIZE, %esp
	andl $CR0_TS_BIT, %eax
	xchgl FPU_CONTEXT_SIZE(%esp), %eax
	jnz .Linterrupts_pushRegisters
	/* Save FPU context */
	fnsave (%esp)
.Linterrupts_pushRegisters:
	pushw %ds
	pushw %es
	pushw %fs
	pushw %gs
	pushl %eax
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %ebp
	pushl %esi
	pushl %edi
	pushl %esp
	subl $4, %esp
	ret
.size interrupts_pushContext, .-interrupts_pushContext


.global interrupts_popContext
.align 4, 0x90
interrupts_popContext:
	popl %esp
	_INTERRUPTS_MULTILOCKCLEAR %eax
.LmultilockCleared:
	/* Set correct gs */
	movw 28(%esp), %dx
	cmpw $SEL_KDATA, %dx
	je .Lignore_gs
	call hal_cpuGetTlsIndex
	shl $3, %eax
	orb $3, %al
	movw %ax, 28(%esp)
.Lignore_gs:
	popl %edi
	popl %esi
	popl %ebp
	popl %edx
	popl %ecx
	popl %ebx
	/* FPU context */
	testl $CR0_TS_BIT, (12 + FPU_CONTEXT_SIZE)(%esp)
	jz .Linterrupts_popFPU
	movl %cr0, %eax
	orl $CR0_TS_BIT, %eax
	movl %eax, %cr0
	jmp .Linterrupts_popFPUend
.Linterrupts_popFPU:
	clts
	frstor 12(%esp)
.Linterrupts_popFPUend:
	popl %eax
	popw %gs
	popw %fs
	popw %es
	popw %ds
	/* Skip over FPU context and cr0Bits. */
	addl $FPU_CONTEXT_SIZE + 4, %esp
	iret
interrupts_popContextUnlocked:
	popl %esp
	jmp .LmultilockCleared
.size interrupts_popContext, .-interrupts_popContext


#define INTERRUPT(name, intr, func) \
.globl name; \
.type name, @function; \
.align 4, 0x90; \
name:; \
	call interrupts_pushContext; \
	movl $SEL_KDATA, %eax; \
	movw %ax, %ds; \
	movw %ax, %es; \
	movw %ax, %fs; \
	movw %ax, %gs; \
	pushl %esp; \
	pushl $intr; \
	call func; \
	movl %eax, %ebx; \
	addl $8, %esp; \
	push $intr; \
	call _interrupts_eoi; \
	addl $4, %esp; \
	testl %ebx, %ebx; \
	jz interrupts_popContextUnlocked; \
	movl %esp, %eax; \
	pushl $0; \
	pushl %eax; \
	pushl $0; \
	call threads_schedule; \
	addl $12, %esp; \
	jmp interrupts_popContext; \
.size name, .-name


INTERRUPT(_interrupts_irq0, 0, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq1, 1, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq2, 2, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq3, 3, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq4, 4, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq5, 5, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq6, 6, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq7, 7, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq8, 8, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq9, 9, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq10, 10, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq11, 11, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq12, 12, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq13, 13, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq14, 14, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_irq15, 15, interrupts_dispatchIRQ)
INTERRUPT(_interrupts_unexpected, 255, _interrupts_unexpected)


.globl _interrupts_TLBShootdown
.type _interrupts_TLBShootdown, @function
.align 4, 0x90
_interrupts_TLBShootdown:
	call interrupts_pushContext
	movl $SEL_KDATA, %eax
	movw %ax, %ds
	movw %ax, %es
	movw %ax, %fs
	movw %ax, %gs
	call hal_tlbShootdown
	pushl $TLB_IRQ
	call _interrupts_eoi
	addl $4, %esp
	jmp interrupts_popContextUnlocked
.size _interrupts_TLBShootdown, .-_interrupts_TLBShootdown


.globl _interrupts_syscall
.type _interrupts_syscall, @function
.align 4, 0x90
_interrupts_syscall:
	call interrupts_pushContext
	movl $SEL_KDATA, %edx
	movw %dx, %ds
	movw %dx, %es
	movl (4 * CTXPUSHL + 12)(%esp), %edx
	pushl %edx
	pushl %eax
	sti
	call syscalls_dispatch
	cli
	addl $8, %esp
	movl %eax, (4 * CTXPUSHL - EAX_OFFSET)(%esp)	/* Save return value to eax in context */
	jmp interrupts_popContextUnlocked
.size _interrupts_syscall, .-_interrupts_syscall


/* int hal_cpuReschedule(struct _spinlock_t *spinlock, spinlock_ctx_t *scp) */
.global	hal_cpuReschedule
.type	hal_cpuReschedule, @function
.align 4, 0x90
hal_cpuReschedule:
	pushl %ebp
	movl %esp, %ebp
	pushl %ebx

	movl 8(%ebp), %ecx     /* ecx := spinlock */
	testl %ecx, %ecx
	je .Lspinlock_null
	/* if (spinlock != NULL): */

	/* optimized call to hal_cpuGetCycles(&spinlock->e) */
	rdtsc                  /* get 64bit timestamp */
	movl %eax, 12(%ecx)    /* LO64(spinlock->e) := eax */
	movl %edx, 16(%ecx)    /* HI64(spinlock->e) := edx */

	/* Calculate maximum and minimum lock time */
.Lupdate_dmax:
	subl 4(%ecx), %eax     /* eax := LO64(spinlock->e - spinlock->b) */
	sbbl 8(%ecx), %edx     /* edx := HI64(spinlock->e - spinlock->b) */

	/* eax:edx := (spinlock->e - spinlock->b) */

	/* if (spinlock->dmax < (spinlock->e - spinlock->b): */
	cmpl 28(%ecx), %eax    /* LO64(spinlock->dmax) */
	movl 32(%ecx), %ebx    /* HI64(spinlock->dmax) */
	sbbl %edx, %ebx
	jnc .Lupdate_dmin

	movl %eax, 28(%ecx)    /* LO64(spinlock->dmax) := eax */
	movl %edx, 32(%ecx)    /* HI64(spinlock->dmax) := edx */

.Lupdate_dmin:
	/* if (spinlock->dmin > (spinlock->e - spinlock->b)) */
	cmpl 20(%ecx), %eax    /* LO64(spinlock->dmin) */
	movl %edx, %ebx
	sbbl 24(%ecx), %ebx    /* HI64(spinlock->dmin) */
	jnc .Lupdate_done

	movl %eax, 20(%ecx)    /* LO64(spinlock->dmin) := eax */
	movl %edx, 24(%ecx)    /* HI64(spinlock->dmin) := edx */

.Lupdate_done:
	movl 12(%ebp), %eax
	pushl (%eax)           /* err := *scp */

.Lspinlock_done:
	cli                    /* hal_cpuDisableInterrupts() */
	pushl %cs
	leal .Lreturn, %eax
	pushl %eax             /* push far cs:address of return */
	xorl %eax, %eax
	call interrupts_pushContext
	mov %esp, %eax
	pushl $0               /* n */
	pushl %eax             /* cpu context */
	pushl $0               /* arg */

	movl 8(%ebp), %ecx     /* ecx := spinlock */
	testl %ecx, %ecx
	jz .Lnospinlock

	call _threads_schedule
	addl $12, %esp

	xorl %eax, %eax
	incl %eax
	movl 8(%ebp), %ecx     /* ecx := spinlock */
	xchgl 44(%ecx), %eax   /* atomic(spinlock->lock <=> eax) */
	jmp interrupts_popContext
.Lnospinlock:
	call threads_schedule
	addl $12, %esp
	jmp interrupts_popContext

.Lspinlock_null:
	pushf                  /* err := cpu_getEFLAGS() */
	jmp .Lspinlock_done

.Lreturn:
	leal -4(%ebp), %esp
	popl %ebx
	popl %ebp
	ret

.size hal_cpuReschedule, .-hal_cpuReschedule
