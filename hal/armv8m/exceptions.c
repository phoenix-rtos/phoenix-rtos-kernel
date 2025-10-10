/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2017, 2022 Phoenix Systems
 * Author: Pawel Pisarczyk, Jakub Sejdak, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/exceptions.h"
#include "hal/cpu.h"
#include "hal/console.h"
#include "hal/string.h"
#include "config.h"

#define CFSR  ((volatile u32 *)0xe000ed28)
#define MMFAR ((volatile u32 *)0xe000ed34)
#define BFAR  ((volatile u32 *)0xe000ed38)

enum exceptions {
	exc_Reset = 1,
	exc_NMI = 2,
	exc_HardFault = 3,
	exc_MemMgtFault = 4,
	exc_BusFault = 5,
	exc_UsageFault = 6,
	exc_SecureFault = 7,
	exc_SVC = 11,
	exc_Debug = 12,
	exc_PendSV = 14,
	exc_SysTick = 15,
};


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, unsigned int n)
{
	static const char *mnemonics[] = {
		"0 #InitialSP", "1 #Reset", "2 #NMI", "3 #HardFault",
		"4 #MemMgtFault", "5 #BusFault", "6 #UsageFault", "7 #SecureFault",
		"8 #", "9 #", "10 #", "11 #SVC",
		"12 #Debug", "13 #", "14 #PendSV", "15 #SysTick"
	};
	size_t i = 0;
	u32 msp = (u32)ctx + sizeof(*ctx);
	u32 psp = ctx->psp;
	u32 cfsr, far;
	cpu_hwContext_t *hwctx;

	/* If we came from userspace HW ctx in on psp stack (according to EXC_RETURN) */
	if ((ctx->excret & (1u << 2)) != 0) {
		hwctx = (void *)ctx->psp;
		msp -= sizeof(cpu_hwContext_t);
		psp += sizeof(cpu_hwContext_t);
	}
	else {
		hwctx = &ctx->mspctx;
	}

	n &= 0xf;

	hal_strcpy(buff, "\nException: ");
	i = sizeof("\nException: ") - 1;
	hal_strcpy(&buff[i], mnemonics[n]);
	i += hal_strlen(mnemonics[n]);

	i += hal_i2s("\n r0=", &buff[i], hwctx->r0, 16, 1);
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

	if (n == exc_BusFault) {
		cfsr = (*CFSR >> 8) & 0xff;
		i += hal_i2s(" bfs=", &buff[i], cfsr, 16, 1);
		/* Check BFARVALID */
		if ((cfsr & 0x80) != 0) {
			far = *BFAR;
			i += hal_i2s("\nbfa=", &buff[i], far, 16, 1);
		}
	}
	else if (n == exc_UsageFault) {
		cfsr = *CFSR >> 16;
		i += hal_i2s(" ufs=", &buff[i], cfsr, 16, 1);
	}
	else if (n == exc_MemMgtFault) {
		cfsr = *CFSR & 0xff;
		i += hal_i2s(" mfs=", &buff[i], cfsr, 16, 1);
		/* Check MMFARVALID */
		if ((cfsr & 0x80) != 0) {
			far = *MMFAR;
			i += hal_i2s("\nmfa=", &buff[i], far, 16, 1);
		}
	}

	buff[i++] = '\n';

	buff[i] = 0;
}


void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
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
}


ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	cpu_hwContext_t *hwctx;

	if ((ctx->excret & (1u << 2)) != 0) {
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
