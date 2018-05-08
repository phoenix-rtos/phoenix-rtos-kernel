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

#include "exceptions.h"
#include "cpu.h"
#include "console.h"
#include "string.h"


static int exceptions_i2s(char *prefix, char *s, unsigned int i, unsigned char b, char zero)
{
	char digits[] = "0123456789abcdef";
	char c;
	unsigned int l, k, m;

	m = hal_strlen(prefix);
	hal_memcpy(s, prefix, m);
	
	for (k = m, l = (unsigned int)-1; l; i /= b, l /= b) {
		if (!zero && !i)
			break;
		s[k++] = digits[i % b];
	}

	l = k--;

	while (k > m) {
		c = s[m];
		s[m++] = s[k];
		s[k--] = c;
	}

	return l;
}


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n)
{
	const char *mnemonics[] = {
		"0 #InitialSP",   "1 #Reset",    "2 #NMI",        "3 #HardFault",
		"4 #MemMgtFault", "5 #BusFault", "6 #UsageFault", "7 #",
		"8 #",            "9 #",         "10 #",          "11 #SVC",
		"12 #Debug",      "13 #",        "14 #PendSV",    "15 #SysTick"
	};
	size_t i = 0;

	hal_strcpy(buff, "\nException: ");
	hal_strcpy(buff += hal_strlen(buff), mnemonics[n]);
	hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i += exceptions_i2s(" r0=", &buff[i], ctx->r0, 16, 1);
	i += exceptions_i2s("  r1=", &buff[i], ctx->r1, 16, 1);
	i += exceptions_i2s("  r2=", &buff[i], ctx->r2, 16, 1);
	i += exceptions_i2s("  r3=", &buff[i], ctx->r3, 16, 1);

	i += exceptions_i2s("\n r4=", &buff[i], ctx->r4, 16, 1);
	i += exceptions_i2s("  r5=", &buff[i], ctx->r5, 16, 1);
	i += exceptions_i2s("  r6=", &buff[i], ctx->r6, 16, 1);
	i += exceptions_i2s("  r7=", &buff[i], ctx->r7, 16, 1);

	i += exceptions_i2s("\n r8=", &buff[i], ctx->r8, 16, 1);
	i += exceptions_i2s("  r9=", &buff[i], ctx->r9, 16, 1);
	i += exceptions_i2s(" r10=", &buff[i], ctx->r10, 16, 1);
	i += exceptions_i2s(" r11=", &buff[i], ctx->r11, 16, 1);

	i += exceptions_i2s("\nr12=", &buff[i], ctx->r12, 16, 1);
	i += exceptions_i2s("  sp=", &buff[i], (u32)ctx - 17 * 4, 16, 1);
	i += exceptions_i2s("  lr=", &buff[i], ctx->lr, 16, 1);
	i += exceptions_i2s("  pc=", &buff[i], ctx->pc, 16, 1);

	i += exceptions_i2s("\npsp=", &buff[i], ctx->psp, 16, 1);
	i += exceptions_i2s(" psr=", &buff[i], ctx->psr, 16, 1);

	i += exceptions_i2s(" cfs=", &buff[i], *(u32*)0xe000ed28, 16, 1);

	buff[i] = 0;
}


void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
{
	char buff[512];

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);
	hal_consolePrint(ATTR_BOLD, "\n");

#ifdef NDEBUG
	hal_cpuRestart();
#else
	hal_cpuHalt();
#endif
}
