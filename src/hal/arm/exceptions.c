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

#include "exceptions.h"
#include "cpu.h"
#include "console.h"
#include "spinlock.h"
#include "string.h"
#include "../../../include/mman.h"


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


static const char digits[] = "0123456789abcdef";


enum { exc_reset = 0, exc_undef, exc_svc, exc_prefetch, exc_abort };


static int exceptions_i2s(char *prefix, char *s, unsigned int i, unsigned char b, char zero)
{
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
		"0 #Reset",       "1 #Undef",    "2 #Syscall",    "3 #Prefetch",
		"4 #Abort",       "5 #Reserved", "6 #FIRQ",       "7 #IRQ"
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
	i += exceptions_i2s("  fp=", &buff[i], ctx->fp, 16, 1);

	i += exceptions_i2s("\n ip=", &buff[i], ctx->ip, 16, 1);
	i += exceptions_i2s("  sp=", &buff[i], (u32)ctx + 21 * 4, 16, 1);
	i += exceptions_i2s("  lr=", &buff[i], ctx->lr, 16, 1);
	i += exceptions_i2s("  pc=", &buff[i], ctx->pc, 16, 1);

	i += exceptions_i2s("\npsr=", &buff[i], ctx->psr, 16, 1);
	i += exceptions_i2s(" dfs=", &buff[i], ctx->dfsr, 16, 1);
	i += exceptions_i2s(" dfa=", &buff[i], ctx->dfar, 16, 1);
	i += exceptions_i2s(" ifs=", &buff[i], ctx->ifsr, 16, 1);

	i += exceptions_i2s("\nifa=", &buff[i], ctx->ifar, 16, 1);

	buff[i] = 0;
}


static void exceptions_defaultHandler(unsigned int n, exc_context_t *ctx)
{
	char buff[512];

	hal_cpuDisableInterrupts();

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);
	hal_consolePrint(ATTR_BOLD, "\n");

	for (;;)
		hal_cpuHalt();
}


void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
{
	if (n == exc_prefetch || n == exc_abort)
		exceptions.abortHandler(n, ctx);
	else if (n == exc_undef)
		exceptions.undefHandler(n, ctx);
	else
		exceptions.defaultHandler(n, ctx);
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

		if (ctx->dfsr & (1 << 11))
			prot |= PROT_WRITE;
	}
	else {
		return PROT_NONE;
	}

	if (status == EXC_PERM_SECTION || status == EXC_PERM_PAGE)
		prot |= PROT_USER;

	return prot;
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
		status != EXC_PERM_SECTION && status != EXC_PERM_PAGE)
		return NULL;

	return addr;
}


int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int, exc_context_t *))
{
	int ret = 0;

	hal_spinlockSet(&exceptions.lock);

	switch (n) {
		case EXC_DEFAULT:
			exceptions.defaultHandler = handler;
			break;

		case EXC_PAGEFAULT:
			exceptions.abortHandler = handler;
			break;

		case EXC_UNDEFINED:
			exceptions.undefHandler = handler;
			break;

		default:
			ret = -1;
			break;
	}

	hal_spinlockClear(&exceptions.lock);

	return ret;
}


void _hal_exceptionsInit(void)
{
	hal_spinlockCreate(&exceptions.lock, "exceptions.lock");

	exceptions.undefHandler = exceptions_defaultHandler;
	exceptions.abortHandler = exceptions_defaultHandler;
	exceptions.defaultHandler = exceptions_defaultHandler;
}
