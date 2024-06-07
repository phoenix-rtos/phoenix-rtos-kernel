/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2012, 2016 Phoenix Systems
 * Copyright 2001, 2005-2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "halsyspage.h"
#include "hal/exceptions.h"
#include "hal/cpu.h"
#include "hal/spinlock.h"
#include "hal/console.h"
#include "hal/string.h"

#include "include/mman.h"
#include "include/errno.h"


/* Exception stubs */
extern void _exceptions_exc0(void);
extern void _exceptions_exc1(void);
extern void _exceptions_exc2(void);
extern void _exceptions_exc3(void);
extern void _exceptions_exc4(void);
extern void _exceptions_exc5(void);
extern void _exceptions_exc6(void);
extern void _exceptions_exc7(void);
extern void _exceptions_exc8(void);
extern void _exceptions_exc9(void);
extern void _exceptions_exc10(void);
extern void _exceptions_exc11(void);
extern void _exceptions_exc12(void);
extern void _exceptions_exc13(void);
extern void _exceptions_exc14(void);
extern void _exceptions_exc15(void);
extern void _exceptions_exc16(void);
extern void _exceptions_exc17(void);
extern void _exceptions_exc18(void);
extern void _exceptions_exc19(void);
extern void _exceptions_exc20(void);
extern void _exceptions_exc21(void);
extern void _exceptions_exc22(void);
extern void _exceptions_exc23(void);
extern void _exceptions_exc24(void);
extern void _exceptions_exc25(void);
extern void _exceptions_exc26(void);
extern void _exceptions_exc27(void);
extern void _exceptions_exc28(void);
extern void _exceptions_exc29(void);
extern void _exceptions_exc30(void);
extern void _exceptions_exc31(void);
extern void exceptions_exc7_handler(unsigned int n, exc_context_t *ctx);

#define SIZE_EXCHANDLERS   32


struct {
	void *handlers[SIZE_EXCHANDLERS];  /* this field should be always first because of assembly stub */
	void (*defaultHandler)(unsigned int, exc_context_t *);
	spinlock_t lock;
} exceptions;


int hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	int prot = PROT_NONE;

	if ((ctx->err & 1) != 0) {
		prot |= PROT_READ;
	}

	if ((ctx->err & 2) != 0) {
		prot |= PROT_WRITE;
	}

	if ((ctx->err & 4) != 0) {
		prot |= PROT_USER;
	}

	return prot;
}


void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	u32 cr2;

	/* clang-format off */
	__asm__ volatile (
		"movl %%cr2, %0"
	: "=r" (cr2)
	:
	: );
	/* clang-format on */

	return (void *)cr2;
}


ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	return ctx->cpuCtx.eip;
}

#ifndef NDEBUG
static unsigned long hal_ld80ToHexString(char *buffer, const u8 *value)
{
	static const char digits[] = "0123456789abcdef";
	size_t i;
	u8 val;
	for (i = 10; i > 0; --i) {
		val = value[i - 1];
		*buffer = digits[val / 16];
		buffer++;
		*buffer = digits[val % 16];
		buffer++;
	}
	return 20;
}
#endif


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n)
{
	static const char *const mnemonics[] = {
		"0 #DE",  "1 #DB",  "2 #NMI", "3 #BP",      "4 #OF",  "5 #BR",  "6 #UD",  "7 #NM",
		"8 #BF",  "9 #",    "10 #TS", "11 #NP",     "12 #SS", "13 #GP", "14 #PF", "15 #",
		"16 #MF", "17 #AC", "18 #MC", "19 #XM/#XF", "20 #VE", "21 #",   "22 #",   "23 #",
		"24 #",   "25 #",   "26 #",   "27 #",       "28 #",   "29 #",   "30 #SE", "31 #" };

	size_t i = 0;
#ifndef NDEBUG
	size_t j;
	u32 cr0, cr3, cr4;
#endif
	u32 ss;

	n &= 0x1f;

	/* clang-format off */
	__asm__ volatile(
		"xorl %0, %0\n\t"
		"movw %%ss, %w0"
	: "=r" (ss)
	:
	: );
	/* clang-format on */

	hal_strcpy(buff, "\nException: ");
	hal_strcpy(buff += hal_strlen(buff), mnemonics[n]);
	hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

#ifndef NDEBUG
	switch (n) {
		case 10: /* #TS */
		case 11: /* #NP */
		case 12: /* #SS */
		case 13: /* #GP */
		case 14: /* #PF */
		case 17: /* #AC */
		case 21: /* #CP */
			i += hal_i2s("err=", &buff[i], ctx->err, 16, 1);
			buff[i++] = '\n';
			break;
		default:
			break;
	}
#endif

	i += hal_i2s("eax=", &buff[i], ctx->cpuCtx.eax, 16, 1);
	i += hal_i2s("  cs=", &buff[i], (u32)ctx->cpuCtx.cs, 16, 1);
	i += hal_i2s(" eip=", &buff[i], ctx->cpuCtx.eip, 16, 1);
	i += hal_i2s(" eflgs=", &buff[i], ctx->cpuCtx.eflags, 16, 1);

	i += hal_i2s("\nebx=", &buff[i], ctx->cpuCtx.ebx, 16, 1);
	i += hal_i2s("  ss=", &buff[i], /*ss != (u32)ctx->ss ? ss : */ (u32)ctx->cpuCtx.ss, 16, 1);
	i += hal_i2s(" esp=", &buff[i], /*ss != (u32)ctx->ss ? (u32)&ctx->eflags + 4 : */ ctx->cpuCtx.esp, 16, 1);
	i += hal_i2s(" ebp=", &buff[i], ctx->cpuCtx.ebp, 16, 1);

	i += hal_i2s("\necx=", &buff[i], ctx->cpuCtx.ecx, 16, 1);
	i += hal_i2s("  ds=", &buff[i], (u32)ctx->cpuCtx.ds, 16, 1);
	i += hal_i2s(" esi=", &buff[i], ctx->cpuCtx.esp, 16, 1);
	i += hal_i2s("  fs=", &buff[i], (u32)ctx->cpuCtx.fs, 16, 1);

	i += hal_i2s("\nedx=", &buff[i], ctx->cpuCtx.edx, 16, 1);
	i += hal_i2s("  es=", &buff[i], (u32)ctx->cpuCtx.es, 16, 1);
	i += hal_i2s(" edi=", &buff[i], ctx->cpuCtx.edi, 16, 1);
	i += hal_i2s("  gs=", &buff[i], (u32)ctx->cpuCtx.gs, 16, 1);

	i += hal_i2s("\ndr0=", &buff[i], ctx->dr0, 16, 1);
	i += hal_i2s(" dr1=", &buff[i], ctx->dr1, 16, 1);
	i += hal_i2s(" dr2=", &buff[i], ctx->dr2, 16, 1);
	i += hal_i2s(" dr3=", &buff[i], ctx->dr3, 16, 1);
	i += hal_i2s("\ndr6=", &buff[i], ctx->dr6, 16, 1);
	i += hal_i2s(" dr7=", &buff[i], ctx->dr7, 16, 1);

	ss = (u32)hal_exceptionsFaultAddr(n, ctx);

	/* i += hal_i2s(" thr=", &buff[i], _proc_current(), 16, 1); */

#ifndef NDEBUG
	/* clang-format off */
	__asm__ volatile (
		"movl %%cr0, %0\n\r"
		"movl %%cr3, %1\n\r"
		"movl %%cr4, %2"
	: "=r" (cr0), "=r" (cr3), "=r" (cr4)
	:
	: );
	/* clang-format on */
	i += hal_i2s("\ncr0=", &buff[i], cr0, 16, 1);
	i += hal_i2s(" cr2=", &buff[i], ss, 16, 1);
	i += hal_i2s(" cr3=", &buff[i], cr3, 16, 1);
	i += hal_i2s(" cr4=", &buff[i], cr4, 16, 1);

	/* Dump FPU context, if it exists */
	if ((ctx->cpuCtx.cr0Bits & CR0_TS_BIT) == 0) {
		i += hal_i2s("\nfcw=", &buff[i], ctx->cpuCtx.fpuContext.controlWord, 16, 1);
		i += hal_i2s(" fsw=", &buff[i], ctx->cpuCtx.fpuContext.statusWord, 16, 1);
		i += hal_i2s(" ftw=", &buff[i], ctx->cpuCtx.fpuContext.tagWord, 16, 1);
		i += hal_i2s(" fip=", &buff[i], ctx->cpuCtx.fpuContext.fip, 16, 1);
		i += hal_i2s("\nfdp=", &buff[i], ctx->cpuCtx.fpuContext.fdp, 16, 1);
		i += hal_i2s(" fds=", &buff[i], ctx->cpuCtx.fpuContext.fds, 16, 1);
		i += hal_i2s(" fips=", &buff[i], ctx->cpuCtx.fpuContext.fips, 16, 1);

		buff[i++] = '\n';
		for (j = 0; j < 8; ++j) {
			i += hal_i2s("fpr", &buff[i], j, 10, 0);
			buff[i++] = '=';
			i += hal_ld80ToHexString(&buff[i], ctx->cpuCtx.fpuContext.fpuContext[j]);
			buff[i++] = ((j & 1) != 0) ? '\n' : ' ';
		}
	}
#else
	i += hal_i2s(" cr2=", &buff[i], ss, 16, 1);
#endif

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
	exceptions.defaultHandler(n, ctx);
}


int hal_exceptionsSetHandler(unsigned int n, void (*handler)(unsigned int, exc_context_t *))
{
	spinlock_ctx_t sc;

	if (n == EXC_DEFAULT) {
		hal_spinlockSet(&exceptions.lock, &sc);
		exceptions.defaultHandler = handler;
		hal_spinlockClear(&exceptions.lock, &sc);

		return EOK;
	}

	if (n >= SIZE_EXCHANDLERS) {
		return -EINVAL;
	}

	hal_spinlockSet(&exceptions.lock, &sc);
	exceptions.handlers[n] = handler;
	hal_spinlockClear(&exceptions.lock, &sc);

	return EOK;
}


/* Function setups interrupt stub in IDT */
__attribute__ ((section (".init"))) void _exceptions_setIDTStub(unsigned int n, void *addr)
{
	u32 w0, w1;
	u32 *idtr;

	w0 = ((u32)addr & 0xffff0000);
	w1 = ((u32)addr & 0x0000ffff);
	w0 |= IGBITS_DPL3 | IGBITS_PRES | IGBITS_SYSTEM | IGBITS_IRQEXC;
	w1 |= (SEL_KCODE << 16);

	idtr = (u32 *)syspage->hs.idtr.addr;
	idtr[n * 2 + 1] = w0;
	idtr[n * 2] = w1;

	return;
}


/* Function initializes exception handling */
__attribute__ ((section (".init"))) void _hal_exceptionsInit(void)
{
	unsigned int k;

	hal_spinlockCreate(&exceptions.lock, "exceptions.lock");
	exceptions.defaultHandler = (void *)exceptions_defaultHandler;

	_exceptions_setIDTStub(0,  _exceptions_exc0);
	_exceptions_setIDTStub(1,  _exceptions_exc1);
	_exceptions_setIDTStub(2,  _exceptions_exc2);
	_exceptions_setIDTStub(3,  _exceptions_exc3);
	_exceptions_setIDTStub(4,  _exceptions_exc4);
	_exceptions_setIDTStub(5,  _exceptions_exc5);
	_exceptions_setIDTStub(6,  _exceptions_exc6);
	_exceptions_setIDTStub(7,  _exceptions_exc7);
	_exceptions_setIDTStub(8,  _exceptions_exc8);
	_exceptions_setIDTStub(9,  _exceptions_exc9);
	_exceptions_setIDTStub(10, _exceptions_exc10);
	_exceptions_setIDTStub(11, _exceptions_exc11);
	_exceptions_setIDTStub(12, _exceptions_exc12);
	_exceptions_setIDTStub(13, _exceptions_exc13);
	_exceptions_setIDTStub(14, _exceptions_exc14);
	_exceptions_setIDTStub(15, _exceptions_exc15);
	_exceptions_setIDTStub(16, _exceptions_exc16);
	_exceptions_setIDTStub(17, _exceptions_exc17);
	_exceptions_setIDTStub(18, _exceptions_exc18);
	_exceptions_setIDTStub(19, _exceptions_exc19);
	_exceptions_setIDTStub(20, _exceptions_exc20);
	_exceptions_setIDTStub(21, _exceptions_exc21);
	_exceptions_setIDTStub(22, _exceptions_exc22);
	_exceptions_setIDTStub(23, _exceptions_exc23);
	_exceptions_setIDTStub(24, _exceptions_exc24);
	_exceptions_setIDTStub(25, _exceptions_exc25);
	_exceptions_setIDTStub(26, _exceptions_exc26);
	_exceptions_setIDTStub(27, _exceptions_exc27);
	_exceptions_setIDTStub(28, _exceptions_exc28);
	_exceptions_setIDTStub(29, _exceptions_exc29);
	_exceptions_setIDTStub(30, _exceptions_exc30);
	_exceptions_setIDTStub(31, _exceptions_exc31);

	for (k = 0; k < SIZE_EXCHANDLERS; k++) {
		exceptions.handlers[k] = exceptions_trampoline;
	}
	exceptions.handlers[7] = exceptions_exc7_handler;

	return;
}
