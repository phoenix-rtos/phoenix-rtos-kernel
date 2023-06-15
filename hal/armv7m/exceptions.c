/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2017 Phoenix Systems
 * Author: Pawel Pisarczyk, Jakub Sejdak
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../exceptions.h"
#include "../cpu.h"
#include "../console.h"
#include "../string.h"
#include "config.h"

#define SIZE_FPUCTX (16 * sizeof(u32))


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n)
{
	static const char *mnemonics[] = {
		"0 #InitialSP",   "1 #Reset",    "2 #NMI",        "3 #HardFault",
		"4 #MemMgtFault", "5 #BusFault", "6 #UsageFault", "7 #",
		"8 #",            "9 #",         "10 #",          "11 #SVC",
		"12 #Debug",      "13 #",        "14 #PendSV",    "15 #SysTick"
	};
	size_t i = 0;
	u32 msp = (u32)ctx + sizeof(*ctx);
	u32 psp = ctx->psp;
	cpu_hwContext_t *hwctx;

	/* If we came from userspace HW ctx in on psp stack */
	if (ctx->excret == RET_THREAD_PSP) {
		hwctx = (void *)ctx->psp;
		msp -= sizeof(cpu_hwContext_t);
		psp += sizeof(cpu_hwContext_t);
#ifdef CPU_IMXRT /* FIXME - check if FPU was enabled instead */
		psp += SIZE_FPUCTX;
#endif
	}
	else {
		hwctx = &ctx->mspctx;
#ifdef CPU_IMXRT
		msp += SIZE_FPUCTX;
#endif
	}

	n &= 0xf;

	hal_strcpy(buff, "\nException: ");
	hal_strcpy(buff += hal_strlen(buff), mnemonics[n]);
	hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i += hal_i2s(" r0=", &buff[i], hwctx->r0, 16, 1);
	i += hal_i2s("  r1=", &buff[i], hwctx->r1, 16, 1);
	i += hal_i2s("  r2=", &buff[i], hwctx->r2, 16, 1);
	i += hal_i2s("  r3=", &buff[i], hwctx->r3, 16, 1);

	i += hal_i2s("\n r4=", &buff[i], ctx->r4, 16, 1);
	i += hal_i2s("  r5=", &buff[i], ctx->r5, 16, 1);
	i += hal_i2s("  r6=", &buff[i], ctx->r6, 16, 1);
	i += hal_i2s("  r7=", &buff[i], ctx->r7, 16, 1);

	i += hal_i2s("\n r8=", &buff[i], ctx->r8, 16, 1);
	i += hal_i2s("  r9=", &buff[i], ctx->r9, 16, 1);
	i += hal_i2s(" r10=", &buff[i], ctx->r10, 16, 1);
	i += hal_i2s(" r11=", &buff[i], ctx->r11, 16, 1);

	i += hal_i2s("\nr12=", &buff[i], hwctx->r12, 16, 1);
	i += hal_i2s(" psr=", &buff[i], hwctx->psr, 16, 1);
	i += hal_i2s("  lr=", &buff[i], hwctx->lr, 16, 1);
	i += hal_i2s("  pc=", &buff[i], hwctx->pc, 16, 1);

	i += hal_i2s("\npsp=", &buff[i], psp, 16, 1);
	i += hal_i2s(" msp=", &buff[i], msp, 16, 1);
	i += hal_i2s(" exr=", &buff[i], ctx->excret, 16, 1);
	i += hal_i2s(" bfa=", &buff[i], *(u32 *)0xe000ed38, 16, 1);

	i += hal_i2s("\ncfs=", &buff[i], *(u32 *)0xe000ed28, 16, 1);

	buff[i++] = '\n';

	buff[i] = 0;
}


void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
{
	char buff[512];

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

#ifndef NDEBUG
	hal_cpuReboot();
#endif

	for (;;) {
		hal_cpuHalt();
	}
}


ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	cpu_hwContext_t *hwctx;

	if (ctx->excret == RET_THREAD_PSP) {
		hwctx = (void *)ctx->psp;
	}
	else {
		hwctx = &ctx->mspctx;
	}

	return hwctx->pc;
}


int hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	return 0;
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
