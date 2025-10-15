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


#define EXC_ASYNC_EXTERNAL      0x16U
#define EXC_PERM_PAGE           0x0fU
#define EXC_SYNC_EXTERNAL_TTW2  0x0eU
#define EXC_PERM_SECTION        0x0dU
#define EXC_SYNC_EXTERNAL_TTW1  0x0cU
#define EXC_DOMAIN_PAGE         0x0bU
#define EXC_DOMAIN_SECTION      0x0aU
#define EXC_SYNC_EXTERNAL       0x08U
#define EXC_TRANSLATION_PAGE    0x07U
#define EXC_ACCESS_PAGE         0x06U
#define EXC_TRANSLATION_SECTION 0x05U
#define EXC_CACHE               0x04U
#define EXC_ACCESS_SECTION      0x03U
#define EXC_DEBUG               0x02U
#define EXC_ALIGMENT            0x01U


static struct {
	void (*undefHandler)(unsigned int n, exc_context_t *ctx);
	void (*abortHandler)(unsigned int n, exc_context_t *ctx);
	void (*defaultHandler)(unsigned int n, exc_context_t *ctx);
	spinlock_t lock;
} exceptions;


/* clang-format off */
enum { exc_reset = 0, exc_undef, exc_svc, exc_prefetch, exc_abort };
/* clang-format on */


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, unsigned int n)
{
	static const char *const mnemonics[] = {
		"0 #Reset", "1 #Undef", "2 #Syscall", "3 #Prefetch",
		"4 #Abort", "5 #Reserved", "6 #FIRQ", "7 #IRQ"
	};
	size_t i = 0;

	n &= 0x7U;

	(void)hal_strcpy(buff, "\nException: ");
	(void)hal_strcpy(buff += hal_strlen(buff), mnemonics[n]);
	(void)hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i += hal_i2s(" r0=", &buff[i], ctx->cpuCtx.r0, 16U, 1U);
	i += hal_i2s("  r1=", &buff[i], ctx->cpuCtx.r1, 16U, 1U);
	i += hal_i2s("  r2=", &buff[i], ctx->cpuCtx.r2, 16U, 1U);
	i += hal_i2s("  r3=", &buff[i], ctx->cpuCtx.r3, 16U, 1U);

	i += hal_i2s("\n r4=", &buff[i], ctx->cpuCtx.r4, 16U, 1U);
	i += hal_i2s("  r5=", &buff[i], ctx->cpuCtx.r5, 16U, 1U);
	i += hal_i2s("  r6=", &buff[i], ctx->cpuCtx.r6, 16U, 1U);
	i += hal_i2s("  r7=", &buff[i], ctx->cpuCtx.r7, 16U, 1U);

	i += hal_i2s("\n r8=", &buff[i], ctx->cpuCtx.r8, 16U, 1U);
	i += hal_i2s("  r9=", &buff[i], ctx->cpuCtx.r9, 16U, 1U);
	i += hal_i2s(" r10=", &buff[i], ctx->cpuCtx.r10, 16U, 1U);
	i += hal_i2s("  fp=", &buff[i], ctx->cpuCtx.fp, 16U, 1U);

	i += hal_i2s("\n ip=", &buff[i], ctx->cpuCtx.ip, 16U, 1U);
	i += hal_i2s("  sp=", &buff[i], ctx->cpuCtx.sp, 16U, 1U);
	i += hal_i2s("  lr=", &buff[i], ctx->cpuCtx.lr, 16U, 1U);
	i += hal_i2s("  pc=", &buff[i], ctx->cpuCtx.pc, 16U, 1U);

	i += hal_i2s("\npsr=", &buff[i], ctx->cpuCtx.psr, 16U, 1U);
	i += hal_i2s(" dfs=", &buff[i], ctx->dfsr, 16U, 1U);
	i += hal_i2s(" dfa=", &buff[i], ctx->dfar, 16U, 1U);
	i += hal_i2s(" ifs=", &buff[i], ctx->ifsr, 16U, 1U);

	i += hal_i2s("\nifa=", &buff[i], ctx->ifar, 16U, 1U);

	buff[i++] = '\n';

	buff[i] = '\0';
}


static void exceptions_defaultHandler(unsigned int n, exc_context_t *ctx)
{
	char buff[SIZE_CTXDUMP];

	hal_cpuDisableInterrupts();

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

#ifdef NDEBUG
	hal_cpuReboot();
#endif

	for (;;) {
		hal_cpuHalt();
	}
}


extern void threads_setupUserReturn(void *retval, cpu_context_t *ctx);

/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "function is used in assembly" */
void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
{
	if (n == (unsigned int)exc_prefetch || n == (unsigned int)exc_abort) {
		exceptions.abortHandler(n, ctx);
	}
	else if (n == (unsigned int)exc_undef) {
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
	unsigned int prot;
	u32 status;

	if (n == (unsigned int)exc_prefetch) {
		prot = PROT_EXEC | PROT_READ;
		status = ctx->ifsr & 0x1fU;
	}
	else if (n == (unsigned int)exc_abort) {
		prot = PROT_READ;
		status = ctx->dfsr & 0x1fU;

		if ((ctx->dfsr & ((unsigned int)0x1 << 11)) != 0U) {
			prot |= PROT_WRITE;
		}
	}
	else {
		return (int)PROT_NONE;
	}

	if (status == EXC_PERM_SECTION || status == EXC_PERM_PAGE) {
		prot |= PROT_USER;
	}

	return (int)prot;
}

ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	return ctx->cpuCtx.pc;
}


void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	u32 status;
	void *addr = NULL;

	if (n == (unsigned int)exc_prefetch) {
		status = ctx->ifsr & 0x1fU;
		addr = (void *)ctx->ifar;
	}
	else if (n == (unsigned int)exc_abort) {
		status = ctx->dfsr & 0x1fU;
		addr = (void *)ctx->dfar;
	}
	else {
		return NULL;
	}

	if (status != EXC_ACCESS_SECTION && status != EXC_ACCESS_PAGE &&
			status != EXC_PERM_SECTION && status != EXC_PERM_PAGE &&
			status != EXC_TRANSLATION_PAGE && status != EXC_TRANSLATION_SECTION) {
		return NULL;
	}

	return addr;
}


int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int n, exc_context_t *ctx))
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
