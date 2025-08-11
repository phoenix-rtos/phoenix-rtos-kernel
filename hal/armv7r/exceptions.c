/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/exceptions.h"
#include "hal/spinlock.h"
#include "hal/cpu.h"
#include "hal/console.h"
#include "hal/string.h"
#include "include/mman.h"
#include "proc/threads.h"


#define EXC_ASYNC_EXTERNAL      0x16
#define EXC_PERM_PAGE           0x0f
#define EXC_SYNC_EXTERNAL_TTW2  0x0e
#define EXC_PERM_SECTION        0x0d
#define EXC_SYNC_EXTERNAL_TTW1  0x0c
#define EXC_DOMAIN_PAGE         0x0b
#define EXC_DOMAIN_SECTION      0x0a
#define EXC_SYNC_EXTERNAL       0x08
#define EXC_TRANSLATION_PAGE    0x07
#define EXC_ACCESS_PAGE         0x06
#define EXC_TRANSLATION_SECTION 0x05
#define EXC_CACHE               0x04
#define EXC_ACCESS_SECTION      0x03
#define EXC_DEBUG               0x02
#define EXC_ALIGMENT            0x01


struct {
	void (*undefHandler)(unsigned int, exc_context_t *);
	void (*abortHandler)(unsigned int, exc_context_t *);
	void (*defaultHandler)(unsigned int, exc_context_t *);
	spinlock_t lock;
} exceptions;


/* clang-format off */
enum { exc_reset = 0, exc_undef, exc_svc, exc_prefetch, exc_abort };
/* clang-format on */


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n)
{
	static const char *const mnemonics[] = {
		"0 #Reset", "1 #Undef", "2 #Syscall", "3 #Prefetch",
		"4 #Abort", "5 #Reserved", "6 #FIRQ", "7 #IRQ"
	};
	size_t i = 0;

	n &= 0x7;

	hal_strcpy(buff, "\nException: ");
	hal_strcpy(buff += hal_strlen(buff), mnemonics[n]);
	hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i += hal_i2s(" r0=", &buff[i], ctx->cpuCtx.r0, 16, 1);
	i += hal_i2s("  r1=", &buff[i], ctx->cpuCtx.r1, 16, 1);
	i += hal_i2s("  r2=", &buff[i], ctx->cpuCtx.r2, 16, 1);
	i += hal_i2s("  r3=", &buff[i], ctx->cpuCtx.r3, 16, 1);

	i += hal_i2s("\n r4=", &buff[i], ctx->cpuCtx.r4, 16, 1);
	i += hal_i2s("  r5=", &buff[i], ctx->cpuCtx.r5, 16, 1);
	i += hal_i2s("  r6=", &buff[i], ctx->cpuCtx.r6, 16, 1);
	i += hal_i2s("  r7=", &buff[i], ctx->cpuCtx.r7, 16, 1);

	i += hal_i2s("\n r8=", &buff[i], ctx->cpuCtx.r8, 16, 1);
	i += hal_i2s("  r9=", &buff[i], ctx->cpuCtx.r9, 16, 1);
	i += hal_i2s(" r10=", &buff[i], ctx->cpuCtx.r10, 16, 1);
	i += hal_i2s("  fp=", &buff[i], ctx->cpuCtx.fp, 16, 1);

	i += hal_i2s("\n ip=", &buff[i], ctx->cpuCtx.ip, 16, 1);
	i += hal_i2s("  sp=", &buff[i], ctx->cpuCtx.sp, 16, 1);
	i += hal_i2s("  lr=", &buff[i], ctx->cpuCtx.lr, 16, 1);
	i += hal_i2s("  pc=", &buff[i], ctx->cpuCtx.pc, 16, 1);

	i += hal_i2s("\npsr=", &buff[i], ctx->cpuCtx.psr, 16, 1);
	i += hal_i2s(" dfs=", &buff[i], ctx->dfsr, 16, 1);
	i += hal_i2s(" dfa=", &buff[i], ctx->dfar, 16, 1);
	i += hal_i2s(" ifs=", &buff[i], ctx->ifsr, 16, 1);

	i += hal_i2s("\nifa=", &buff[i], ctx->ifar, 16, 1);

	buff[i++] = '\n';

	buff[i] = 0;
}


static void exceptions_defaultHandler(unsigned int n, exc_context_t *ctx)
{
	char buff[512];

	hal_cpuDisableInterrupts();

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

#ifdef NDEBUG
	hal_cpuReboot();
#endif

	proc_crash(proc_current());
	proc_threadEnd();
}


extern void threads_setupUserReturn(void *retval, cpu_context_t *ctx);


void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
{
	if (n == exc_prefetch || n == exc_abort) {
		exceptions.abortHandler(n, ctx);
	}
	else if (n == exc_undef) {
		exceptions.undefHandler(n, ctx);
	}
	else {
		exceptions.defaultHandler(n, ctx);
	}

	/* Handle signals if necessary */
	if (hal_cpuSupervisorMode(&ctx->cpuCtx) == 0) {
		threads_setupUserReturn((void *)ctx->cpuCtx.r0, &ctx->cpuCtx);
	}
}


int hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	int prot;
	u32 status;

	if (n == exc_prefetch) {
		prot = PROT_EXEC | PROT_READ;
		status = ctx->ifsr & 0x1f;
	}
	else if (n == exc_abort) {
		prot = PROT_READ;
		status = ctx->dfsr & 0x1f;

		if (ctx->dfsr & (1 << 11)) {
			prot |= PROT_WRITE;
		}
	}
	else {
		return PROT_NONE;
	}

	if (status == EXC_PERM_SECTION || status == EXC_PERM_PAGE) {
		prot |= PROT_USER;
	}

	return prot;
}

ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	return ctx->cpuCtx.pc;
}


void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	u32 status;
	void *addr = NULL;

	if (n == exc_prefetch) {
		status = ctx->ifsr & 0x1f;
		addr = (void *)ctx->ifar;
	}
	else if (n == exc_abort) {
		status = ctx->dfsr & 0x1f;
		addr = (void *)ctx->dfar;
	}
	else {
		return NULL;
	}

	if (status != EXC_ACCESS_SECTION && status != EXC_ACCESS_PAGE &&
			status != EXC_PERM_SECTION && status != EXC_PERM_PAGE &&
			status != EXC_TRANSLATION_PAGE && status != EXC_TRANSLATION_SECTION)
		return NULL;

	return addr;
}


int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int, exc_context_t *))
{
	int ret = 0;
	spinlock_ctx_t sc;

	hal_spinlockSet(&exceptions.lock, &sc);

	switch (n) {
		case EXC_DEFAULT:
			exceptions.defaultHandler = handler;
			break;

		case EXC_UNDEFINED:
			exceptions.undefHandler = handler;
			break;

		default:
			ret = -1;
			break;
	}

	hal_spinlockClear(&exceptions.lock, &sc);

	return ret;
}


void _hal_exceptionsInit(void)
{
	hal_spinlockCreate(&exceptions.lock, "exceptions.lock");

	exceptions.undefHandler = exceptions_defaultHandler;
	exceptions.abortHandler = exceptions_defaultHandler;
	exceptions.defaultHandler = exceptions_defaultHandler;
}
