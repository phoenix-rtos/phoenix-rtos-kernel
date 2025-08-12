/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Low-level initialization for Cortex-M55 (ARMv8) architecture
 *
 * Copyright 2012, 2016-2017, 2025 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <arch/cpu.h>

#define SCS_BASE  0xe000e000u
#define SCS_CPACR 0xd88u
#define SCS_ICSR  0xd04u
#define SCS_VTOR  0xd08u
#define SCS_FPCCR 0xf34u
#define SCS_FPCAR 0xf38u


#if KERNEL_FPU_SUPPORT
#define HWCTX_SIZE (((8 + 18) * 4))
#define ISRCTX_SIZE ((14 + 16) * 4)
/* If CPU is running in secure mode and using FPU we need to set SFPA bit in CONTROL */
#define CONTROL_SFPA (1 << 3)
#else
#define HWCTX_SIZE (8 * 4)
#define ISRCTX_SIZE (14 * 4)
#define CONTROL_SFPA 0
#endif

.syntax unified

.extern syscalls
.extern syspage


.section .init, "x"

.globl _init_vectors
.type _init_vectors, %object
_init_vectors:
.word _end + 1024 + 256
.word _start

.word _exceptions_dispatch /* NMI */
.word _exceptions_dispatch /* HardFault */
.word _exceptions_dispatch /* MemMgtFault */
.word _exceptions_dispatch /* BusFault */
.word _exceptions_dispatch /* UsageFault */
.word _exceptions_dispatch /* SecureFault */
.word 0
.word 0
.word 0
.word _syscall_dispatch    /* SVC */
.word _exceptions_dispatch /* Debug */
.word 0
.word _interrupts_dispatch /* PendSV */
.word _interrupts_dispatch /* Systick */

.rept 195                  /* Max number of ext interrupts - last peripheral id + 1 */
.word _interrupts_dispatch
.endr
.size _init_vectors, .-_init_vectors


.thumb
.thumb_func

.globl _start
.type _start, %function

_start:
	cpsid if
	isb

	/* Point to syspage */
	ldr r8, =syspage
	str r9, [r8]

	/* Init vector table pointer */
	ldr r7, =SCS_BASE
	adr r1, _init_vectors
	str r1, [r7, #SCS_VTOR]
	isb
	dmb

	/* Init MSP to a first value in _init_vectors (.bss end + 1024 + 256) */
	ldr r0, [r1]
	bic r0, #7
	msr msp, r0
	/* These registers are used for hardware checking of stack limits.
	 * This feature is currently not implemented in our kernel for ARMv8-M,
	 * setting them to 0 essentially disables this feature. */
	movs r0, #0
	msr msplim, r0
	msr psplim, r0
	isb

#if KERNEL_FPU_SUPPORT
	/* Enable FPU and MVE. We have a function _hal_scsFPUSet, but we can't use it yet
	 * and we have to untrap FPU early - _stm32_init may get compiled to code with MVE. */
	mov r0, #(0x3 << 20)
	str r0, [r7, #SCS_CPACR]
	isb

	/* Enable lazy floating-point state preservation */
	ldr r0, [r7, #SCS_FPCCR]
	orr r0, #(1 << 30)
	str r0, [r7, #SCS_FPCCR]
	isb
#else
	/* Trap FPU and MVE, in the chosen setup kernel doesn't support them. */
	movs r0, #0
	str r0, [r7, #SCS_CPACR]
	isb
#endif

	movs r0, #0 /* Switch to main stack, privileged mode */
	msr control, r0

	bl _stm32_init

	bl main
	/* If main() returns, wait here infinitely for thread to be rescheduled */
	b .
.size _start, .-_start
.ltorg

.globl _syscall_dispatch
.type _syscall_dispatch, %function
.type _syscallend, %function

_syscall_dispatch:
	mov r3, sp              /* Store MSP for later */
	sub sp, sp, #HWCTX_SIZE /* Reserve space for copying hardware-stacked context */
	mov r1, sp              /* Address to copy hardware-stacked context */
	mrs r0, psp

#if KERNEL_FPU_SUPPORT
	vpush {s16-s31}
#endif
	str r3, [sp, #-8]! /* store msp, skip pad0 */
	mov r12, #0
	push {r0, r4-r12} /* store psp, r4~r11, irq_ret */

	mov r10, r1
	/* Registers:
	 * r0    - hw-stacked context on process stack
	 * r1~r8 - userspace r0~r3, r12, lr, pc, psr
	 * r10   - hw-stacked context on kernel stack
	 */

	ldmia r0!, {r1-r8}
	orr r7, r7, #1 /* fix PC LSB not being set */
	stmia r10!, {r1-r8}
	/* r5~r8 free for use */

#if KERNEL_FPU_SUPPORT
	/* copy s0-s15, fpscr, vpr */
	vldmia r0!, {s0-s17}
	vstmia r10!, {s0-s17}
#endif
	/* r10 free for use */

	sub sp, sp, #8
	str sp, [sp] /* savesp */

#if KERNEL_FPU_SUPPORT
	ldr r6, =SCS_BASE
	ldr r5, [r6, #SCS_FPCAR]
	str r5, [sp, #4] /* fpuctx */

	/* At this point full context is saved on kernel stack.
	 * Point FPCAR to the s0 register in saved context. */
	add r5, sp, #(ISRCTX_SIZE + (8 * 4))
	str r5, [r6, #SCS_FPCAR]
	dmb
	isb
#endif

	/* syscalls_dispatch expects arguments to be on the user stack.
	 * Push userspace r0~r3 onto ustack. */
	stmdb r0!, {r1-r4}

	/* Prepare arguments for function call:
	 * void *syscalls_dispatch(int n, char *ustack, cpu_context_t *ctx) */
	mov r1, r0
	mov r2, sp
	ldrb r0, [r7, #-3] /* get syscall number by examining SVC instruction */

	/* Prepare pseudo context that will call syscalls_dispatch */
	adr r5, _syscallend
	ldr r6, =syscalls_dispatch
	ldr r7, =DEFAULT_PSR
	push {r0-r7} /* r0~r3, r12, lr, pc, psr */

	/* Set Thread mode to privileged execution */
	mrs r0, control
	bic r0, r0, #0x3 /* Switch to main stack, privileged mode */
	msr control, r0
	isb

	/* Exit Handler mode to kernel Thread mode, set FType because our pseudo-context doesn't have FPU */
	ldr r0, =(RET_THREAD_MSP | EXC_RETURN_FTYPE)
	bx r0

_syscallend:
	/* return value in r0 */
	add sp, sp, #(2 * 4)  /* Skip over savesp, fpuctx */
	pop {r1, r4-r11, lr}  /* load psp, restore r4~r11, lr (ignored) */
	adds r1, r1, #HWCTX_SIZE
	msr psp, r1
	isb

	/* Get lr, pc from the context on stack */
	ldrd lr, r3, [sp, #(ISRCTX_SIZE - (12 * 4) + (5 * 4))]

#if KERNEL_FPU_SUPPORT
	add sp, sp, #(2 * 4) /* Skip over irq_ret, msp, pad0 */
	vpop {s16-s31}
	add sp, sp, #(8 * 4) /* Skip over hwctx (only the integer part) */
	vpop {s0-s15}
	pop {r1-r2}
	vmsr fpscr, r1
	vmsr vpr, r2
#else
	add sp, sp, #((2 * 4) + HWCTX_SIZE) /* Skip over irq_ret, msp, pad0 and hwctx */
#endif

	/* Switch stacks */
	mrs r1, control
	orr r1, r1, #0x3 /* Switch to process stack, unprivileged mode */
	msr control, r1
	isb

	bx r3
.size _syscall_dispatch, .-_syscall_dispatch
.ltorg

.globl _exceptions_dispatch
.type _exceptions_dispatch, %function

_exceptions_dispatch:
	cpsid if
	isb

	mov r0, sp /* save MSP to be stored in cpu_context_t */
	tst lr, #EXC_RETURN_SPSEL
	itt ne
	subne sp, sp, #(8 * 4) /* If coming from userspace, make room on the stack for hardware-stored context */
	bne 1f

	tst lr, #EXC_RETURN_FTYPE /* If coming from kernel, calculate msp as it was before the exception  */
	ite ne
	addne r0, r0, #(8 * 4)
	addeq r0, r0, #((8 + 18) * 4)

1:
#if KERNEL_FPU_SUPPORT
	vpush {s16-s31}
#endif
	str r0, [sp, #-8]! /* Store msp, skip over pad0 */

	mrs r0, ipsr
	sub r1, sp, #(12 * 4) /* pointer to cpu_context_t on stack */
#if KERNEL_FPU_SUPPORT
	ldr r2, =SCS_BASE
	ldr r2, [r2, #SCS_FPCAR]
#endif
	mrs r3, psp
	push {r1-r11, lr} /* store savesp, fpuctx, psp, r4~r11, irq_ret */

	/* void exceptions_dispatch(unsigned int n, exc_context_t *ctx) */
	b exceptions_dispatch
.size _exceptions_dispatch, .-_exceptions_dispatch
.ltorg

.globl _interrupts_dispatch
.type _interrupts_dispatch, %function
_interrupts_dispatch:
	mov r0, sp /* save MSP */
	tst lr, #EXC_RETURN_SPSEL
	it ne
	subne sp, sp, #HWCTX_SIZE /* space corresponding to hw-saved ctx */

#if KERNEL_FPU_SUPPORT
	vpush {s16-s31}
#endif
	str r0, [sp, #-8]! /* store msp, skip pad0 */

	mrs r0, ipsr
	sub r1, sp, #(12 * 4) /* pointer to cpu_context_t on stack */
#if KERNEL_FPU_SUPPORT
	ldr r2, =SCS_BASE
	ldr r2, [r2, #SCS_FPCAR]
#endif
	mrs r3, psp
	push {r1-r11, lr} /* store savesp, fpuctx, psp, r4~r11, irq_ret */

	/* if we came from userspace, copy hw-saved ctx to kstack in case of signal handling */
	beq .Lskip_hwctx_copy /* tst lr, #EXC_RETURN_SPSEL */

	/* psp in r3 - load hw-saved ctx */
	add r12, sp, #ISRCTX_SIZE
#if KERNEL_FPU_SUPPORT
	vldm r3, {s0-s25}
	vstm r12, {s0-s25}
#else
	ldm r3, {r4-r11}
	stm r12, {r4-r11}
#endif

.Lskip_hwctx_copy:
	/* void interrupts_dispatch(unsigned int n, cpu_context_t *ctx) */
	bl interrupts_dispatch

	/* Switch stack to savesp to restore context from there */
	ldr sp, [sp]
	isb
#if KERNEL_FPU_SUPPORT
	/* If we have FPU, we can load r4~r11 right away, because we will use FPU registers to copy over hw-saved ctx */
	pop {r1-r11, lr}          /* load savesp (unused), fpuctx, psp, r4~r11, lr */
	ldr r0, [sp], #8          /* load msp, skip pad0 */
	tst lr, #EXC_RETURN_SPSEL /* check if we are returning to userspace */
	beq .Lskip_hwctx_restore

	add r12, sp, #(16 * 4) /* skip s16~s31 */
	vldm r12, {s0-s25}     /* load hwcontext, s0~s15, fpscr and vpr */
	vstm r3, {s0-s25}      /* store them at psp */

.Lskip_hwctx_restore:
	vpop {s16-s31}
	isb

	mov sp, r0 /* restore msp */

	ldr r1, =SCS_BASE
	str r2, [r1, #SCS_FPCAR]
	dsb
	isb
#else
	/* If we don't have FPU, we need 8 registers to copy over hw-saved ctx, so load r4~r11 later */
	ldr lr, [sp, #(11 * 4)]   /* load lr */
	tst lr, #EXC_RETURN_SPSEL /* check if we are returning to userspace */
	beq .Lskip_hwctx_restore

	/* userspace return - restore registers in case of signal handling */
	ldr r3, [sp, #(2 * 4)] /* load psp */
	add r12, sp, #ISRCTX_SIZE
	ldm r12, {r4-r11}
	stm r3, {r4-r11}

.Lskip_hwctx_restore:
	add sp, sp, #(2 * 4) /* skip over savesp, fpuctx - both unused  */
	pop {r3-r11}         /* load psp, r4~r11 */
	ldr sp, [sp, #4]     /* restore msp */
#endif
	msr psp, r3
	isb

	/* Check if we're returning to userspace and pick the right CONTROL value */
	mrs r1, control
	ite ne /* tst lr, #EXC_RETURN_SPSEL */
	orrne r1, r1, #0x3 /* Switch to process stack, unprivileged mode */
	biceq r1, r1, #0x3 /* Switch to main stack, privileged mode */
	msr control, r1
	dsb
	isb

	bx lr
.size _interrupts_dispatch, .-_interrupts_dispatch
.ltorg


.globl hal_cpuReschedule /* int hal_cpuReschedule(struct _spinlock_t *spinlock, spinlock_ctx_t *scp); */
.type hal_cpuReschedule, %function
hal_cpuReschedule:
	/* r0 - spinlock
	 * r1 - spinlock context (ignored, because we always want to re-enable interrupts) */

	/* Invoke PendSV to perform a reschedule */
	ldr r2, =SCS_BASE
	ldr r1, [r2, #SCS_ICSR]
	orr r1, #(1 << 28)  /* ICSR.PENDSVSET */
	str r1, [r2, #SCS_ICSR]

	/* If spinlock not NULL, clear it */
	cbz r0, .Lhal_cpuReschedule0
	adds r0, r0, #12
.Lspinlock:
	/* TODO: this can be done better using acquire/release instructions */
	ldrexb r2, [r0]
	adds r2, r2, #1
	dmb
	strexb r1, r2, [r0]
	cmp r1, #0
	bne .Lspinlock
.Lhal_cpuReschedule0:
	movs r0, #0 /* default return value */
	cpsie if
	isb
	dmb
	bx lr
.size hal_cpuReschedule, .-hal_cpuReschedule
.ltorg


.globl hal_jmp /* void hal_jmp(void *f, void *kstack, void *ustack, int kargc, const arg_t *kargs) */
.type hal_jmp, %function
hal_jmp:
	cpsid if
	isb

	cbnz r2, hal_jmp_user

	/* kargs has been passed on the stack */
	ldr r5, [sp]
	mov r4, r0
	mov sp, r1
	isb
	subs r3, #1
	bmi 1f
	ldr r0, [r5]
	subs r3, #1
	bmi 1f
	ldr r1, [r5, #4]
	subs r3, #1
	bmi 1f
	ldr r2, [r5, #8]
	subs r3, #1
	bmi 1f
	ldr r3, [r5, #12]
1:
	cpsie if
	isb
	dmb
	bx r4

hal_jmp_user:
	cpsid if
	isb
	msr msp, r1
	isb
	msr psp, r2
	cpsie if
	isb
	movs r1, #(USERCONTROL | CONTROL_SFPA)
	msr control, r1
	isb
	dmb
	bx r0

.size hal_jmp, .-hal_jmp
.ltorg

.globl hal_exceptionJump /* void hal_exceptionJump(unsigned int n, exc_context_t *ctx, void (*handler)(unsigned int, exc_context_t *)) */
.type hal_exceptionJump, %function
.type .L_hal_exceptionJump_end, %function
hal_exceptionJump:
	push {r4-r11, lr}
	/* Prepare pseudo context that will call handler */
	ldr r7, =DEFAULT_PSR
	mov r6, r2
	adr r5, .L_hal_exceptionJump_end
	push {r0-r7} /* r0~r3, r12, lr, pc, psr */

	mrs r0, control
	bic r0, r0, #0x3 /* Switch to main stack, privileged mode */
	msr control, r0
	isb
	/* Exit Handler mode to kernel Thread mode, set FType because our pseudo-context doesn't have FPU */
	ldr r0, =(RET_THREAD_MSP | EXC_RETURN_FTYPE)
	cpsie if
	dsb
	bx r0

.L_hal_exceptionJump_end:
	pop {r4-r11, pc}

.size hal_exceptionJump, .-hal_exceptionJump
.ltorg
