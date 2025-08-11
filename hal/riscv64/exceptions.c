/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling (RISCV64)
 *
 * Copyright 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/exceptions.h"
#include "hal/cpu.h"
#include "hal/spinlock.h"
#include "hal/console.h"
#include "hal/string.h"
#include "proc/threads.h"

#include "include/mman.h"

#define SIZE_EXCEPTIONS 16U


static struct {
	excHandlerFn_t handlers[SIZE_EXCEPTIONS];
	excHandlerFn_t defaultHandler;
	spinlock_t spinlock;
} exceptions_common;


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, unsigned int n)
{
	unsigned long i = 0;

	static const char *mnemonics[] = {
		"0 Instruction address missaligned", "1 Instruction access fault", "2 Illegal instruction", "3 Breakpoint",
		"4 Reserved", "5 Load access fault", "6 AMO address misaligned", "7 Store/AMO access fault",
		"8 Environment call", "9 Reserved", "10 Reserved", "11 Reserved",
		"12 Instruction page fault", "13 Load page fault", "14 Reserved", "15 Store/AMO page fault"
	};

	n &= 0xfU;

	(void)hal_strcpy(buff, "\nException: ");
	(void)hal_strcpy(buff += hal_strlen(buff), mnemonics[n]);
	(void)hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i += hal_i2s("zero: ", &buff[i], 0, 16U, 1U);
	i += hal_i2s("  ra : ", &buff[i], (u64)ctx->ra, 16U, 1U);
	i += hal_i2s("   sp : ", &buff[i], (u64)ctx->sp, 16U, 1U);
	i += hal_i2s("   gp : ", &buff[i], (u64)ctx->gp, 16U, 1U);
	buff[i++] = '\n';

	i += hal_i2s(" tp : ", &buff[i], (u64)ctx->tp, 16U, 1U);
	i += hal_i2s("  t0 : ", &buff[i], (u64)ctx->t0, 16U, 1U);
	i += hal_i2s("   t1 : ", &buff[i], (u64)ctx->t1, 16U, 1U);
	i += hal_i2s("   t2 : ", &buff[i], (u64)ctx->t2, 16U, 1U);
	buff[i++] = '\n';

	i += hal_i2s(" s0 : ", &buff[i], (u64)ctx->s0, 16U, 1U);
	i += hal_i2s("  s1 : ", &buff[i], (u64)ctx->s1, 16U, 1U);
	i += hal_i2s("   a0 : ", &buff[i], (u64)ctx->a0, 16U, 1U);
	i += hal_i2s("   a1 : ", &buff[i], (u64)ctx->a1, 16U, 1U);
	buff[i++] = '\n';

	i += hal_i2s(" a2 : ", &buff[i], (u64)ctx->a2, 16U, 1U);
	i += hal_i2s("  a3 : ", &buff[i], (u64)ctx->a3, 16U, 1U);
	i += hal_i2s("   a4 : ", &buff[i], (u64)ctx->a4, 16U, 1U);
	i += hal_i2s("   a5 : ", &buff[i], (u64)ctx->a5, 16U, 1U);
	buff[i++] = '\n';

	i += hal_i2s(" a6 : ", &buff[i], (u64)ctx->a6, 16U, 1U);
	i += hal_i2s("  a7 : ", &buff[i], (u64)ctx->a7, 16U, 1U);
	i += hal_i2s("   s2 : ", &buff[i], (u64)ctx->s2, 16U, 1U);
	i += hal_i2s("   s3 : ", &buff[i], (u64)ctx->s3, 16U, 1U);
	buff[i++] = '\n';

	i += hal_i2s(" s4 : ", &buff[i], (u64)ctx->s4, 16U, 1U);
	i += hal_i2s("  s5 : ", &buff[i], (u64)ctx->s5, 16U, 1U);
	i += hal_i2s("   s6 : ", &buff[i], (u64)ctx->s6, 16U, 1U);
	i += hal_i2s("   s7 : ", &buff[i], (u64)ctx->s7, 16U, 1U);
	buff[i++] = '\n';

	i += hal_i2s(" s8 : ", &buff[i], (u64)ctx->s8, 16U, 1U);
	i += hal_i2s("  s9 : ", &buff[i], (u64)ctx->s9, 16U, 1U);
	i += hal_i2s("  s10 : ", &buff[i], (u64)ctx->s10, 16U, 1U);
	i += hal_i2s("  s11 : ", &buff[i], (u64)ctx->s11, 16U, 1U);
	buff[i++] = '\n';

	i += hal_i2s(" t3 : ", &buff[i], (u64)ctx->t3, 16U, 1U);
	i += hal_i2s("  t4 : ", &buff[i], (u64)ctx->t4, 16U, 1U);
	i += hal_i2s("   t5 : ", &buff[i], (u64)ctx->t5, 16U, 1U);
	i += hal_i2s("   t6 : ", &buff[i], (u64)ctx->t6, 16U, 1U);
	buff[i++] = '\n';

	i += hal_i2s(" ksp : ", &buff[i], (u64)ctx->ksp, 16U, 1U);
	i += hal_i2s(" sstatus : ", &buff[i], (u64)ctx->sstatus, 16U, 1U);
	i += hal_i2s(" sepc : ", &buff[i], (u64)ctx->sepc, 16U, 1U);
	buff[i++] = '\n';
	i += hal_i2s(" stval : ", &buff[i], (u64)ctx->stval, 16U, 1U);
	i += hal_i2s(" scause : ", &buff[i], (u64)ctx->scause, 16U, 1U);
	i += hal_i2s(" sscratch : ", &buff[i], (u64)ctx->sscratch, 16U, 1U);
	buff[i++] = '\n';
	i += hal_i2s(" cpu id : ", &buff[i], hal_cpuGetID(), 16U, 0U);

	buff[i++] = '\n';

	buff[i] = '\0';
}


static void exceptions_defaultHandler(unsigned int n, exc_context_t *ctx)
{
	char buff[SIZE_CTXDUMP];

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

#ifdef NDEBUG
	hal_cpuReboot();
#else
	proc_crash(proc_current());
	proc_threadEnd();
#endif
}


static void exceptions_trampoline(unsigned int n, exc_context_t *ctx)
{
	exceptions_common.defaultHandler(n, ctx);
}


vm_prot_t hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	vm_prot_t prot = PROT_NONE;
	u64 cause = ctx->scause;

	(void)n;

	prot |= PROT_READ;

	if ((cause == 6U) || (cause == 7U) || (cause == 15U)) {
		prot |= PROT_WRITE;
	}

	if ((cause <= 3U) || (cause == 12U)) {
		prot |= PROT_EXEC;
	}

	if ((ctx->sstatus & 0x100U) == 0U) {
		/* from user code */
		prot |= PROT_USER;
	}

	return prot;
}


void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	return (void *)ctx->stval;
}


ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	return ctx->sepc;
}


void threads_setupUserReturn(void *retval, cpu_context_t *ctx);


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Usage in assembly" */
void exceptions_dispatch(unsigned int n, cpu_context_t *ctx)
{
	spinlock_ctx_t sc;
	excHandlerFn_t h;

	if (n >= SIZE_EXCEPTIONS) {
		return;
	}

	hal_spinlockSet(&exceptions_common.spinlock, &sc);
	h = exceptions_common.handlers[n];
	hal_spinlockClear(&exceptions_common.spinlock, &sc);

	h(n, ctx);

	/* Handle signals if necessary */
	if (hal_cpuSupervisorMode(ctx) == 0) {
		threads_setupUserReturn((void *)ctx->a0, ctx);
	}
}


int hal_exceptionsSetHandler(unsigned int n, excHandlerFn_t handler)
{
	spinlock_ctx_t sc;

	if (n == EXC_DEFAULT) {
		hal_spinlockSet(&exceptions_common.spinlock, &sc);
		exceptions_common.defaultHandler = handler;
		hal_spinlockClear(&exceptions_common.spinlock, &sc);

		return 0;
	}

	if (n == EXC_PAGEFAULT) {
		hal_spinlockSet(&exceptions_common.spinlock, &sc);
		exceptions_common.handlers[12] = handler;
		exceptions_common.handlers[13] = handler;
		exceptions_common.handlers[15] = handler;
		hal_spinlockClear(&exceptions_common.spinlock, &sc);

		return 0;
	}

	if (n >= SIZE_EXCEPTIONS) {
		return -1;
	}

	hal_spinlockSet(&exceptions_common.spinlock, &sc);
	exceptions_common.handlers[n] = handler;
	hal_spinlockClear(&exceptions_common.spinlock, &sc);

	return 0;
}


/* Function initializes exception handling */
__attribute__((section(".init"))) void _hal_exceptionsInit(void)
{
	unsigned int k;

	hal_spinlockCreate(&exceptions_common.spinlock, "exceptions_common.spinlock");
	exceptions_common.defaultHandler = exceptions_defaultHandler;

	for (k = 0; k < SIZE_EXCEPTIONS; k++) {
		exceptions_common.handlers[k] = exceptions_trampoline;
	}
}
