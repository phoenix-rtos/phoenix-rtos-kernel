/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
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
#include "hal/sparcv8leon3/srmmu.h"
#include "include/mman.h"


static struct {
	void (*defaultHandler)(unsigned int, exc_context_t *);
	void (*mmuFaultHandler)(unsigned int, exc_context_t *);
	spinlock_t lock;
} exceptions_common;


static const char *const hal_exceptionsType(int n)
{
	switch (n) {
		case 0x0:
			return " #Reset";
		case 0x1:
			return " #Page fault - instruction fetch";
		case 0x2:
			return " #Illegal instruction";
		case 0x3:
			return " #Privileged instruction";
		case 0x4:
			return " #FP disabled";
		case 0x7:
			return " #Address not aligned";
		case 0x8:
			return " #FP exception";
		case 0x9:
			return " #Page fault - data access";
		case 0xa:
			return " #Tag overflow";
		case 0xb:
			return " #Watchpoint";
		case 0x2b:
			return " #Data store error";
		case 0x81:
			return " #Breakpoint";
		case 0x82:
			return " #Division by zero";
		case 0x84:
			return " #Clean windows";
		case 0x85:
			return " #Range check";
		case 0x86:
			return " #Fix alignment";
		case 0x87:
			return " #Integer overflow";
		case 0x88:
			return " #Syscall (unimplemented)";
		default:
			return " #Reserved/Unknown";
	}
}


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n)
{
	cpu_winContext_t *win = (cpu_winContext_t *)ctx->cpuCtx.sp;
	size_t i = hal_i2s("\033[0m\nException: 0x", buff, n, 16, 0);
	buff[i] = '\0';

	hal_strcpy(buff += hal_strlen(buff), hal_exceptionsType(n));
	hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i = 0;
	i += hal_i2s(" g0=", &buff[i], 0, 16, 1);
	i += hal_i2s(" g1=", &buff[i], ctx->cpuCtx.g1, 16, 1);
	i += hal_i2s(" g2=", &buff[i], ctx->cpuCtx.g2, 16, 1);
	i += hal_i2s(" g3=", &buff[i], ctx->cpuCtx.g3, 16, 1);
	i += hal_i2s("\n g4=", &buff[i], ctx->cpuCtx.g4, 16, 1);
	i += hal_i2s(" g5=", &buff[i], ctx->cpuCtx.g5, 16, 1);
	i += hal_i2s(" g6=", &buff[i], ctx->cpuCtx.g6, 16, 1);
	i += hal_i2s(" g7=", &buff[i], ctx->cpuCtx.g7, 16, 1);

	i += hal_i2s("\n o0=", &buff[i], ctx->cpuCtx.o0, 16, 1);
	i += hal_i2s(" o1=", &buff[i], ctx->cpuCtx.o1, 16, 1);
	i += hal_i2s(" o2=", &buff[i], ctx->cpuCtx.o2, 16, 1);
	i += hal_i2s(" o3=", &buff[i], ctx->cpuCtx.o3, 16, 1);
	i += hal_i2s("\n o4=", &buff[i], ctx->cpuCtx.o4, 16, 1);
	i += hal_i2s(" o5=", &buff[i], ctx->cpuCtx.o5, 16, 1);
	i += hal_i2s(" sp=", &buff[i], ctx->cpuCtx.sp, 16, 1);
	i += hal_i2s(" o7=", &buff[i], ctx->cpuCtx.o7, 16, 1);

	i += hal_i2s("\n l0=", &buff[i], win->l0, 16, 1);
	i += hal_i2s(" l1=", &buff[i], win->l1, 16, 1);
	i += hal_i2s(" l2=", &buff[i], win->l2, 16, 1);
	i += hal_i2s(" l3=", &buff[i], win->l3, 16, 1);
	i += hal_i2s("\n l4=", &buff[i], win->l4, 16, 1);
	i += hal_i2s(" l5=", &buff[i], win->l5, 16, 1);
	i += hal_i2s(" l6=", &buff[i], win->l6, 16, 1);
	i += hal_i2s(" l7=", &buff[i], win->l7, 16, 1);

	i += hal_i2s("\n i0=", &buff[i], win->i0, 16, 1);
	i += hal_i2s(" i1=", &buff[i], win->i1, 16, 1);
	i += hal_i2s(" i2=", &buff[i], win->i2, 16, 1);
	i += hal_i2s(" i3=", &buff[i], win->i3, 16, 1);
	i += hal_i2s("\n i4=", &buff[i], win->i4, 16, 1);
	i += hal_i2s(" i5=", &buff[i], win->i5, 16, 1);
	i += hal_i2s(" fp=", &buff[i], win->fp, 16, 1);
	i += hal_i2s(" i7=", &buff[i], win->i7, 16, 1);

	i += hal_i2s("\n y=", &buff[i], ctx->cpuCtx.y, 16, 1);
	i += hal_i2s(" psr=", &buff[i], ctx->cpuCtx.psr, 16, 1);
	i += hal_i2s(" wim=", &buff[i], ctx->wim, 16, 1);
	i += hal_i2s(" tbr=", &buff[i], ctx->tbr, 16, 1);
	i += hal_i2s("\n pc=", &buff[i], ctx->cpuCtx.pc, 16, 1);
	i += hal_i2s(" npc=", &buff[i], ctx->cpuCtx.npc, 16, 1);
	buff[i++] = '\n';
	buff[i] = '\0';
}


__attribute__((noreturn)) static void exceptions_defaultHandler(unsigned int n, exc_context_t *ctx)
{
	char buff[SIZE_CTXDUMP];

	hal_cpuDisableInterrupts();

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

#ifdef NDEBUG
	// hal_cpuReboot();
#endif

	for (;;) {
		hal_cpuHalt();
	}

	__builtin_unreachable();
}


extern void threads_setupUserReturn(void *retval);


void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
{
	if ((n == EXC_PAGEFAULT) || (n == EXC_PAGEFAULT_DATA)) {
		exceptions_common.mmuFaultHandler(n, ctx);
	}
	else {
		exceptions_common.defaultHandler(n, ctx);
	}

	/* Handle signals if necessary */
	if (hal_cpuSupervisorMode(&ctx->cpuCtx) == 0) {
		threads_setupUserReturn((void *)ctx->cpuCtx.o0);
	}
}


int hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	if ((n == EXC_PAGEFAULT) || (n == EXC_PAGEFAULT_DATA)) {
		return hal_srmmuGetFaultSts();
	}

	return 0;
}


ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	return ctx->cpuCtx.pc;
}


void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	return (void *)hal_srmmuGetFaultAddr();
}


int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int, exc_context_t *))
{
	if ((n == EXC_PAGEFAULT) || (n == EXC_PAGEFAULT_DATA)) {
		exceptions_common.mmuFaultHandler = handler;
	}
	else {
		exceptions_common.defaultHandler = handler;
	}

	return 0;
}

void _hal_exceptionsInit(void)
{
	hal_spinlockCreate(&exceptions_common.lock, "exceptions.lock");

	exceptions_common.defaultHandler = exceptions_defaultHandler;
	exceptions_common.mmuFaultHandler = exceptions_defaultHandler;
}
