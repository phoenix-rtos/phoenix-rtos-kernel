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

#include "include/mman.h"
#include "proc/coredump.h"
#include "proc/elf.h"

#define SIZE_EXCEPTIONS 16


static struct {
	void (*handlers[SIZE_EXCEPTIONS])(unsigned int, exc_context_t *);
	void (*defaultHandler)(unsigned int, exc_context_t *);
	spinlock_t spinlock;
} exceptions_common;


const char *hal_exceptionMnemonic(int n)
{
	static const char *mnemonics[] = {
		"0 Instruction address missaligned", "1 Instruction access fault", "2 Illegal instruction", "3 Breakpoint",
		"4 Reserved", "5 Load access fault", "6 AMO address misaligned", "7 Store/AMO access fault",
		"8 Environment call", "9 Reserved", "10 Reserved", "11 Reserved",
		"12 Instruction page fault", "13 Load page fault", "14 Reserved", "15 Store/AMO page fault"
	};

	return mnemonics[n & 0xf];
}


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n)
{
	unsigned int i = 0;

	hal_strcpy(buff, "\nException: ");
	hal_strcpy(buff += hal_strlen(buff), hal_exceptionMnemonic(n));
	hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i += hal_i2s("zero: ", &buff[i], 0, 16, 1);
	i += hal_i2s("  ra : ", &buff[i], (u64)ctx->ra, 16, 1);
	i += hal_i2s("   sp : ", &buff[i], (u64)ctx->sp, 16, 1);
	i += hal_i2s("   gp : ", &buff[i], (u64)ctx->gp, 16, 1);
	buff[i++] = '\n';

	i += hal_i2s(" tp : ", &buff[i], (u64)ctx->tp, 16, 1);
	i += hal_i2s("  t0 : ", &buff[i], (u64)ctx->t0, 16, 1);
	i += hal_i2s("   t1 : ", &buff[i], (u64)ctx->t1, 16, 1);
	i += hal_i2s("   t2 : ", &buff[i], (u64)ctx->t2, 16, 1);
	buff[i++] = '\n';

	i += hal_i2s(" s0 : ", &buff[i], (u64)ctx->s0, 16, 1);
	i += hal_i2s("  s1 : ", &buff[i], (u64)ctx->s1, 16, 1);
	i += hal_i2s("   a0 : ", &buff[i], (u64)ctx->a0, 16, 1);
	i += hal_i2s("   a1 : ", &buff[i], (u64)ctx->a1, 16, 1);
	buff[i++] = '\n';

	i += hal_i2s(" a2 : ", &buff[i], (u64)ctx->a2, 16, 1);
	i += hal_i2s("  a3 : ", &buff[i], (u64)ctx->a3, 16, 1);
	i += hal_i2s("   a4 : ", &buff[i], (u64)ctx->a4, 16, 1);
	i += hal_i2s("   a5 : ", &buff[i], (u64)ctx->a5, 16, 1);
	buff[i++] = '\n';

	i += hal_i2s(" a6 : ", &buff[i], (u64)ctx->a6, 16, 1);
	i += hal_i2s("  a7 : ", &buff[i], (u64)ctx->a7, 16, 1);
	i += hal_i2s("   s2 : ", &buff[i], (u64)ctx->s2, 16, 1);
	i += hal_i2s("   s3 : ", &buff[i], (u64)ctx->s3, 16, 1);
	buff[i++] = '\n';

	i += hal_i2s(" s4 : ", &buff[i], (u64)ctx->s4, 16, 1);
	i += hal_i2s("  s5 : ", &buff[i], (u64)ctx->s5, 16, 1);
	i += hal_i2s("   s6 : ", &buff[i], (u64)ctx->s6, 16, 1);
	i += hal_i2s("   s7 : ", &buff[i], (u64)ctx->s7, 16, 1);
	buff[i++] = '\n';

	i += hal_i2s(" s8 : ", &buff[i], (u64)ctx->s8, 16, 1);
	i += hal_i2s("  s9 : ", &buff[i], (u64)ctx->s9, 16, 1);
	i += hal_i2s("  s10 : ", &buff[i], (u64)ctx->s10, 16, 1);
	i += hal_i2s("  s11 : ", &buff[i], (u64)ctx->s11, 16, 1);
	buff[i++] = '\n';

	i += hal_i2s(" t3 : ", &buff[i], (u64)ctx->t3, 16, 1);
	i += hal_i2s("  t4 : ", &buff[i], (u64)ctx->t4, 16, 1);
	i += hal_i2s("   t5 : ", &buff[i], (u64)ctx->t5, 16, 1);
	i += hal_i2s("   t6 : ", &buff[i], (u64)ctx->t6, 16, 1);
	buff[i++] = '\n';

	i += hal_i2s(" ksp : ", &buff[i], (u64)ctx->ksp, 16, 1);
	i += hal_i2s(" sstatus : ", &buff[i], (u64)ctx->sstatus, 16, 1);
	i += hal_i2s(" sepc : ", &buff[i], (u64)ctx->sepc, 16, 1);
	buff[i++] = '\n';
	i += hal_i2s(" stval : ", &buff[i], (u64)ctx->stval, 16, 1);
	i += hal_i2s(" scause : ", &buff[i], (u64)ctx->scause, 16, 1);
	i += hal_i2s(" sscratch : ", &buff[i], (u64)ctx->sscratch, 16, 1);
	buff[i++] = '\n';
	i += hal_i2s(" cpu id : ", &buff[i], hal_cpuGetID(), 16, 0);

	buff[i++] = '\n';

	buff[i] = 0;
}


static void exceptions_defaultHandler(unsigned int n, exc_context_t *ctx)
{
	char buff[SIZE_CTXDUMP];

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

	coredump_dump(n, ctx);

#ifdef NDEBUG
	hal_cpuReboot();
#endif

	for (;;) {
		hal_cpuHalt();
	}
}


static void exceptions_trampoline(unsigned int n, exc_context_t *ctx)
{
	exceptions_common.defaultHandler(n, ctx);
}


int hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	int prot = PROT_NONE;
	u64 cause = ctx->scause;

	(void)n;

	prot |= PROT_READ;

	if ((cause == 6) || (cause == 7) || (cause == 15)) {
		prot |= PROT_WRITE;
	}

	if ((cause <= 3) || (cause == 12)) {
		prot |= PROT_EXEC;
	}

	if ((ctx->sstatus & 0x100) == 0) {
		/* from user code */
		prot |= PROT_USER;
	}

	return prot;
}


inline void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	return (void *)ctx->stval;
}


inline ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	return ctx->sepc;
}


extern void threads_setupUserReturn(void *retval, cpu_context_t *ctx);


void exceptions_dispatch(unsigned int n, cpu_context_t *ctx)
{
	spinlock_ctx_t sc;

	void (*h)(unsigned int, exc_context_t *);

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


int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int, exc_context_t *))
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
	exceptions_common.defaultHandler = (void *)exceptions_defaultHandler;

	for (k = 0; k < SIZE_EXCEPTIONS; k++) {
		exceptions_common.handlers[k] = exceptions_trampoline;
	}
}


cpu_context_t *hal_excToCpuCtx(exc_context_t *ctx)
{
	return ctx;
}


void hal_coredumpGRegset(void *buff, cpu_context_t *ctx)
{
	u64 *regs = (u64 *)buff;

	*(regs++) = ctx->sepc;
	*(regs++) = ctx->ra;
	*(regs++) = ctx->sp;
	*(regs++) = ctx->gp;
	*(regs++) = ctx->tp;
	*(regs++) = ctx->t0;
	*(regs++) = ctx->t1;
	*(regs++) = ctx->t2;
	*(regs++) = ctx->s0;
	*(regs++) = ctx->s1;
	*(regs++) = ctx->a0;
	*(regs++) = ctx->a1;
	*(regs++) = ctx->a2;
	*(regs++) = ctx->a3;
	*(regs++) = ctx->a4;
	*(regs++) = ctx->a5;
	*(regs++) = ctx->a6;
	*(regs++) = ctx->a7;
	*(regs++) = ctx->s2;
	*(regs++) = ctx->s3;
	*(regs++) = ctx->s4;
	*(regs++) = ctx->s5;
	*(regs++) = ctx->s6;
	*(regs++) = ctx->s7;
	*(regs++) = ctx->s8;
	*(regs++) = ctx->s9;
	*(regs++) = ctx->s10;
	*(regs++) = ctx->s11;
	*(regs++) = ctx->t3;
	*(regs++) = ctx->t4;
	*(regs++) = ctx->t5;
	*(regs++) = ctx->t6;
}


void hal_coredumpThreadAux(void *buff, cpu_context_t *ctx)
{
#ifdef PROC_COREDUMP_FPUCTX
	static const char FPREGSET_NAME[] = "CORE";
	Elf64_Nhdr nhdr;
	nhdr.n_namesz = sizeof(FPREGSET_NAME);
	nhdr.n_descsz = sizeof(ctx->fpCtx);
	nhdr.n_type = NT_FPREGSET;
	hal_memcpy(buff, &nhdr, sizeof(nhdr));
	buff = (char *)buff + sizeof(nhdr);
	hal_memcpy(buff, FPREGSET_NAME, sizeof(FPREGSET_NAME));
	buff = (char *)buff + ((sizeof(FPREGSET_NAME) + 3) & ~3);

	hal_memcpy(buff, &ctx->fpCtx, sizeof(ctx->fpCtx));
#endif
}


void hal_coredumpGeneralAux(void *buff)
{
}
