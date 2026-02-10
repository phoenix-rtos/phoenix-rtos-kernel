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
#include "include/mman.h"
#include "proc/threads.h"

/* clang-format off */
#define EXCEPTION_PREFIX "\033" "[0m" "\nException: 0x"
/* clang-format on */


static const char *const hal_exceptionsType(unsigned int n)
{
	switch (n) {
		case 0x0U:
			return " #Reset";
		case 0x1U:
			return " #Page fault - instruction fetch";
		case 0x2U:
			return " #Illegal instruction";
		case 0x3U:
			return " #Privileged instruction";
		case 0x4U:
			return " #FP disabled";
		case 0x7U:
			return " #Address not aligned";
		case 0x8U:
			return " #FP exception";
		case 0x9U:
			return " #Page fault - data load";
		case 0xaU:
			return " #Tag overflow";
		case 0xbU:
			return " #Watchpoint";
		case 0x2bU:
			return " #Data store error";
		case 0x81U:
			return " #Breakpoint";
		case 0x82U:
			return " #Division by zero";
		case 0x84U:
			return " #Clean windows";
		case 0x85U:
			return " #Range check";
		case 0x86U:
			return " #Fix alignment";
		case 0x87U:
			return " #Integer overflow";
		case 0x88U:
			return " #Syscall (unimplemented)";
		default:
			return " #Reserved/Unknown";
	}
}


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, unsigned int n)
{
	cpu_winContext_t *win = (cpu_winContext_t *)ctx->cpuCtx.sp;
	size_t i = hal_i2s(EXCEPTION_PREFIX, buff, n, 16U, 0U);
	buff[i] = '\0';

	(void)hal_strcpy(buff += hal_strlen(buff), hal_exceptionsType(n));
	(void)hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i = 0;
	i += hal_i2s(" g0=", &buff[i], 0, 16, 1);
	i += hal_i2s(" g1=", &buff[i], ctx->cpuCtx.g1, 16U, 1U);
	i += hal_i2s(" g2=", &buff[i], ctx->cpuCtx.g2, 16U, 1U);
	i += hal_i2s(" g3=", &buff[i], ctx->cpuCtx.g3, 16U, 1U);
	i += hal_i2s("\n g4=", &buff[i], ctx->cpuCtx.g4, 16U, 1U);
	i += hal_i2s(" g5=", &buff[i], ctx->cpuCtx.g5, 16U, 1U);
	i += hal_i2s(" g6=", &buff[i], ctx->cpuCtx.g6, 16U, 1U);
	i += hal_i2s(" g7=", &buff[i], ctx->cpuCtx.g7, 16U, 1U);

	i += hal_i2s("\n o0=", &buff[i], ctx->cpuCtx.o0, 16U, 1U);
	i += hal_i2s(" o1=", &buff[i], ctx->cpuCtx.o1, 16U, 1U);
	i += hal_i2s(" o2=", &buff[i], ctx->cpuCtx.o2, 16U, 1U);
	i += hal_i2s(" o3=", &buff[i], ctx->cpuCtx.o3, 16U, 1U);
	i += hal_i2s("\n o4=", &buff[i], ctx->cpuCtx.o4, 16U, 1U);
	i += hal_i2s(" o5=", &buff[i], ctx->cpuCtx.o5, 16U, 1U);
	i += hal_i2s(" sp=", &buff[i], ctx->cpuCtx.sp, 16U, 1U);
	i += hal_i2s(" o7=", &buff[i], ctx->cpuCtx.o7, 16U, 1U);

	i += hal_i2s("\n l0=", &buff[i], win->l0, 16U, 1U);
	i += hal_i2s(" l1=", &buff[i], win->l1, 16U, 1U);
	i += hal_i2s(" l2=", &buff[i], win->l2, 16U, 1U);
	i += hal_i2s(" l3=", &buff[i], win->l3, 16U, 1U);
	i += hal_i2s("\n l4=", &buff[i], win->l4, 16U, 1U);
	i += hal_i2s(" l5=", &buff[i], win->l5, 16U, 1U);
	i += hal_i2s(" l6=", &buff[i], win->l6, 16U, 1U);
	i += hal_i2s(" l7=", &buff[i], win->l7, 16U, 1U);

	i += hal_i2s("\n i0=", &buff[i], win->i0, 16U, 1U);
	i += hal_i2s(" i1=", &buff[i], win->i1, 16U, 1U);
	i += hal_i2s(" i2=", &buff[i], win->i2, 16U, 1U);
	i += hal_i2s(" i3=", &buff[i], win->i3, 16U, 1U);
	i += hal_i2s("\n i4=", &buff[i], win->i4, 16U, 1U);
	i += hal_i2s(" i5=", &buff[i], win->i5, 16U, 1U);
	i += hal_i2s(" fp=", &buff[i], win->fp, 16U, 1U);
	i += hal_i2s(" i7=", &buff[i], win->i7, 16U, 1U);

	i += hal_i2s("\n y=", &buff[i], ctx->cpuCtx.y, 16U, 1U);
	i += hal_i2s(" psr=", &buff[i], ctx->cpuCtx.psr, 16U, 1U);
	i += hal_i2s(" wim=", &buff[i], ctx->wim, 16U, 1U);
	i += hal_i2s(" tbr=", &buff[i], ctx->tbr, 16U, 1U);
	i += hal_i2s("\n pc=", &buff[i], ctx->cpuCtx.pc, 16U, 1U);
	i += hal_i2s(" npc=", &buff[i], ctx->cpuCtx.npc, 16U, 1U);
	buff[i++] = '\n';
	buff[i] = '\0';
}


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Usage in assembly" */
void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
{
	char buff[SIZE_CTXDUMP];

	hal_cpuDisableInterrupts();

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

#ifdef NDEBUG
	hal_cpuReboot();
#else
	proc_crash(proc_current());
	proc_threadEnd();
#endif
}


vm_prot_t hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	return 0;
}


ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	return ctx->cpuCtx.pc;
}


void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	return NULL;
}


int hal_exceptionsSetHandler(unsigned int n, excHandlerFn_t handler)
{
	return 0;
}

void _hal_exceptionsInit(void)
{
}
