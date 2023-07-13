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

#include "../exceptions.h"
#include "../spinlock.h"
#include "../cpu.h"
#include "../console.h"
#include "../string.h"
#include "../../include/mman.h"


static const char *const hal_exceptionsType(int n)
{
	switch (n) {
		case 0x0:
			return "0 #Reset";
		case 0x1:
			return "1 #Page fault - instruction fetch";
		case 0x2:
			return "2 #Illegal instruction";
		case 0x3:
			return "3 #Privileged instruction";
		case 0x4:
			return "4 #FP disabled";
		case 0x7:
			return "7 #Address not aligned";
		case 0x8:
			return "8 #FP exception";
		case 0x9:
			return "9 #Page fault - data load";
		case 0xa:
			return "10 #Tag overflow";
		case 0xb:
			return "11 #Watchpoint";
		case 0x81:
			return "0x81 #Breakpoint";
		case 0x82:
			return "0x82 #Division by zero";
		case 0x84:
			return "0x84 #Clean windows";
		case 0x85:
			return "0x85 #Range check";
		case 0x86:
			return "0x86 #Fix alignment";
		case 0x87:
			return "0x87 #Integer overflow";
		case 0x88:
			return "0x88 #Syscall (unimplemented)";
		default:
			return " #Reserved/Unknown";
	}
}


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n)
{
	size_t i = 0;
	hal_strcpy(buff, "\033[0m\nException: ");
	hal_strcpy(buff += hal_strlen(buff), hal_exceptionsType(n));
	hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i += hal_i2s(" g0=", &buff[i], ctx->g0, 16, 1);
	i += hal_i2s(" g1=", &buff[i], ctx->g1, 16, 1);
	i += hal_i2s(" g2=", &buff[i], ctx->g2, 16, 1);
	i += hal_i2s(" g3=", &buff[i], ctx->g3, 16, 1);
	i += hal_i2s("\n g4=", &buff[i], ctx->g4, 16, 1);
	i += hal_i2s(" g5=", &buff[i], ctx->g5, 16, 1);
	i += hal_i2s(" g6=", &buff[i], ctx->g6, 16, 1);
	i += hal_i2s(" g7=", &buff[i], ctx->g7, 16, 1);

	i += hal_i2s("\n o0=", &buff[i], ctx->o0, 16, 1);
	i += hal_i2s(" o1=", &buff[i], ctx->o1, 16, 1);
	i += hal_i2s(" o2=", &buff[i], ctx->o2, 16, 1);
	i += hal_i2s(" o3=", &buff[i], ctx->o3, 16, 1);
	i += hal_i2s("\n o4=", &buff[i], ctx->o4, 16, 1);
	i += hal_i2s(" o5=", &buff[i], ctx->o5, 16, 1);
	i += hal_i2s(" sp=", &buff[i], ctx->sp, 16, 1);
	i += hal_i2s(" o7=", &buff[i], ctx->o7, 16, 1);

	i += hal_i2s("\n l0=", &buff[i], ctx->l0, 16, 1);
	i += hal_i2s(" l1=", &buff[i], ctx->l1, 16, 1);
	i += hal_i2s(" l2=", &buff[i], ctx->l2, 16, 1);
	i += hal_i2s(" l3=", &buff[i], ctx->l3, 16, 1);
	i += hal_i2s("\n l4=", &buff[i], ctx->l4, 16, 1);
	i += hal_i2s(" l5=", &buff[i], ctx->l5, 16, 1);
	i += hal_i2s(" l6=", &buff[i], ctx->l6, 16, 1);
	i += hal_i2s(" l7=", &buff[i], ctx->l7, 16, 1);

	i += hal_i2s("\n i0=", &buff[i], ctx->i0, 16, 1);
	i += hal_i2s(" i1=", &buff[i], ctx->i1, 16, 1);
	i += hal_i2s(" i2=", &buff[i], ctx->i2, 16, 1);
	i += hal_i2s(" i3=", &buff[i], ctx->i3, 16, 1);
	i += hal_i2s("\n i4=", &buff[i], ctx->i4, 16, 1);
	i += hal_i2s(" i5=", &buff[i], ctx->i5, 16, 1);
	i += hal_i2s(" fp=", &buff[i], ctx->fp, 16, 1);
	i += hal_i2s(" i7=", &buff[i], ctx->i7, 16, 1);

	i += hal_i2s("\n y=", &buff[i], ctx->y, 16, 1);
	i += hal_i2s(" psr=", &buff[i], ctx->psr, 16, 1);
	i += hal_i2s(" wim=", &buff[i], ctx->wim, 16, 1);
	i += hal_i2s(" tbr=", &buff[i], ctx->tbr, 16, 1);
	i += hal_i2s("\n pc=", &buff[i], ctx->pc, 16, 1);
	i += hal_i2s(" npc=", &buff[i], ctx->npc, 16, 1);
	buff[i++] = '\n';
	buff[i] = '\0';
}


void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
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


int hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	return 0;
}


ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	return ctx->pc;
}


void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	return NULL;
}


int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int, exc_context_t *))
{
	return 0;
}

void _hal_exceptionsInit(void)
{
}
