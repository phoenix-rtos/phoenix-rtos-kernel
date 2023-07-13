/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling (RISCV64)
 *
 * Copyright 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../exceptions.h"
#include "../cpu.h"
#include "../spinlock.h"
#include "../console.h"
#include "../string.h"

#include "../../include/mman.h"

#define SIZE_EXCEPTIONS   16


struct {
	void (*handlers[SIZE_EXCEPTIONS])(unsigned int, exc_context_t *);
	void (*defaultHandler)(unsigned int, exc_context_t *);
	spinlock_t spinlock;
} exceptions_common;


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n)
{
	unsigned int i = 0;

	static const char *mnemonics[] = {
		"0 Instruction adrress missaligned", "1 Instruction access fault",  "2 Illegal instruction", "3 Breakpoint",
		"4 Reserved",  "5 Load access fault",  "6 AMO address misaligned",  "7 Store/AMO access fault",
		"8 Environment call",  "9 Reserved",    "10 Reserved", "11 Reserved",
		"12 Instruction page fault", "13 Load page fault", "14 Reserved", "15 Store/AMO page fault" };

	n &= 0xf;

	hal_strcpy(buff, "\nException: ");
	hal_strcpy(buff += hal_strlen(buff), mnemonics[n]);
	hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i += hal_i2s("zero: ", &buff[i], 0, 16, 1);
	i += hal_i2s("  ra : ", &buff[i], (u64)ctx->pc, 16, 1);
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
	i += hal_i2s(" sbaddaddr : ", &buff[i], (u64)ctx->sbadaddr, 16, 1);
	i += hal_i2s(" scause : ", &buff[i], (u64)ctx->scause, 16, 1);
	i += hal_i2s(" sscratch : ", &buff[i], (u64)ctx->sscratch, 16, 1);

	buff[i++] = '\n';

	buff[i] = 0;

	return;
}


static void exceptions_defaultHandler(unsigned int n, exc_context_t *ctx)
{
	char buff[SIZE_CTXDUMP];

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

#ifdef NDEBUG
	hal_cpuReboot();
#endif

	for (;;) {
		hal_cpuHalt();
	}

	return;
}


static void exceptions_trampoline(unsigned int n, exc_context_t *ctx)
{
	exceptions_common.defaultHandler(n, ctx);
}


int hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	int prot = PROT_NONE;

	u64 num = ctx->scause;

	prot |= PROT_READ;

	if (num == 6 || num== 7 || num == 15)
		prot |= PROT_WRITE;

	if (num <= 3 || num == 12)
		prot |= PROT_EXEC;

	if ((ctx->sstatus & 0x100) == 0) // from user code
		prot |= PROT_USER;

	return prot;
}


inline void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	return (void *)ctx->sbadaddr;
}


inline ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	return ctx->pc;
}


void exceptions_dispatch(unsigned int n, cpu_context_t *ctx)
{
	spinlock_ctx_t sc;

	void (*h)(unsigned int, exc_context_t *);

	if (n >= SIZE_EXCEPTIONS)
		return;

	hal_spinlockSet(&exceptions_common.spinlock, &sc);
	h = exceptions_common.handlers[n];
	hal_spinlockClear(&exceptions_common.spinlock, &sc);

	h(n, ctx);

	return;
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

	if (n >= SIZE_EXCEPTIONS)
		return -1;

	hal_spinlockSet(&exceptions_common.spinlock, &sc);
	exceptions_common.handlers[n] = handler;
	hal_spinlockClear(&exceptions_common.spinlock, &sc);

	return 0;
}


/* Function initializes exception handling */
__attribute__ ((section (".init"))) void _hal_exceptionsInit(void)
{
	unsigned int k;

	hal_spinlockCreate(&exceptions_common.spinlock, "exceptions_common.spinlock");
	exceptions_common.defaultHandler = (void *)exceptions_defaultHandler;

	for (k = 0; k < SIZE_EXCEPTIONS; k++)
		exceptions_common.handlers[k] = exceptions_trampoline;

	return;
}
