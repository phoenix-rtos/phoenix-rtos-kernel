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
#include "proc/threads.h"


/* Exception stubs */
/* parasoft-begin-suppress MISRAC2012-RULE_8_6 "Definitions in assembly" */
void _exceptions_exc0(void);
void _exceptions_exc1(void);
void _exceptions_exc2(void);
void _exceptions_exc3(void);
void _exceptions_exc4(void);
void _exceptions_exc5(void);
void _exceptions_exc6(void);
void _exceptions_exc7(void);
void _exceptions_exc8(void);
void _exceptions_exc9(void);
void _exceptions_exc10(void);
void _exceptions_exc11(void);
void _exceptions_exc12(void);
void _exceptions_exc13(void);
void _exceptions_exc14(void);
void _exceptions_exc15(void);
void _exceptions_exc16(void);
void _exceptions_exc17(void);
void _exceptions_exc18(void);
void _exceptions_exc19(void);
void _exceptions_exc20(void);
void _exceptions_exc21(void);
void _exceptions_exc22(void);
void _exceptions_exc23(void);
void _exceptions_exc24(void);
void _exceptions_exc25(void);
void _exceptions_exc26(void);
void _exceptions_exc27(void);
void _exceptions_exc28(void);
void _exceptions_exc29(void);
void _exceptions_exc30(void);
void _exceptions_exc31(void);
void exceptions_exc7_handler(unsigned int n, exc_context_t *ctx);
/* parasoft-end-suppress MISRAC2012-RULE_8_6 */


#define SIZE_EXCHANDLERS 32U


/* parasoft-begin-suppress MISRAC2012-RULE_8_4 "Struct accessed from assembly" */
struct {
	excHandlerFn_t handlers[SIZE_EXCHANDLERS]; /* this field should be always first because of assembly stub */
	excHandlerFn_t defaultHandler;
	spinlock_t lock;
} exceptions;
/* parasoft-end-suppress MISRAC2012-RULE_8_4 */


vm_prot_t hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	vm_prot_t prot = PROT_NONE;

	if ((ctx->err & 1U) != 0U) {
		prot |= PROT_READ;
	}

	if ((ctx->err & 2U) != 0U) {
		prot |= PROT_WRITE;
	}

	if ((ctx->err & 4U) != 0U) {
		prot |= PROT_USER;
	}

	return prot;
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly required for low-level operations" */
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


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly required for low-level operations" */
void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, unsigned int n)
{
	static const char *const mnemonics[] = {
		"0 #DE", "1 #DB", "2 #NMI", "3 #BP", "4 #OF", "5 #BR", "6 #UD", "7 #NM",
		"8 #BF", "9 #", "10 #TS", "11 #NP", "12 #SS", "13 #GP", "14 #PF", "15 #",
		"16 #MF", "17 #AC", "18 #MC", "19 #XM/#XF", "20 #VE", "21 #", "22 #", "23 #",
		"24 #", "25 #", "26 #", "27 #", "28 #", "29 #", "30 #SE", "31 #"
	};

	size_t i = 0;
	u32 ss;

	n &= 0x1fU;

	/* clang-format off */
	__asm__ volatile(
		"xorl %0, %0\n\t"
		"movw %%ss, %w0"
	: "=r" (ss)
	:
	: );
	/* clang-format on */

	(void)hal_strcpy(buff, "\nException: ");
	(void)hal_strcpy(buff += hal_strlen(buff), mnemonics[n]);
	(void)hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i += hal_i2s("eax=", &buff[i], ctx->cpuCtx.eax, 16U, 1U);
	i += hal_i2s("  cs=", &buff[i], (u32)ctx->cpuCtx.cs, 16U, 1U);
	i += hal_i2s(" eip=", &buff[i], ctx->cpuCtx.eip, 16U, 1U);
	i += hal_i2s(" eflgs=", &buff[i], ctx->cpuCtx.eflags, 16U, 1U);

	i += hal_i2s("\nebx=", &buff[i], ctx->cpuCtx.ebx, 16U, 1U);
	i += hal_i2s("  ss=", &buff[i], /*ss != (u32)ctx->ss ? ss : */ (u32)ctx->cpuCtx.ss, 16U, 1U);
	i += hal_i2s(" esp=", &buff[i], /*ss != (u32)ctx->ss ? (u32)&ctx->eflags + 4 : */ ctx->cpuCtx.esp, 16U, 1U);
	i += hal_i2s(" ebp=", &buff[i], ctx->cpuCtx.ebp, 16U, 1U);

	i += hal_i2s("\necx=", &buff[i], ctx->cpuCtx.ecx, 16U, 1U);
	i += hal_i2s("  ds=", &buff[i], (u32)ctx->cpuCtx.ds, 16U, 1U);
	i += hal_i2s(" esi=", &buff[i], ctx->cpuCtx.esp, 16U, 1U);
	i += hal_i2s("  fs=", &buff[i], (u32)ctx->cpuCtx.fs, 16U, 1U);

	i += hal_i2s("\nedx=", &buff[i], ctx->cpuCtx.edx, 16U, 1U);
	i += hal_i2s("  es=", &buff[i], (u32)ctx->cpuCtx.es, 16U, 1U);
	i += hal_i2s(" edi=", &buff[i], ctx->cpuCtx.edi, 16U, 1U);
	i += hal_i2s("  gs=", &buff[i], (u32)ctx->cpuCtx.gs, 16U, 1U);

	i += hal_i2s("\ndr0=", &buff[i], ctx->dr0, 16U, 1U);
	i += hal_i2s(" dr1=", &buff[i], ctx->dr1, 16U, 1U);
	i += hal_i2s(" dr2=", &buff[i], ctx->dr2, 16U, 1U);
	i += hal_i2s(" dr3=", &buff[i], ctx->dr3, 16U, 1U);
	i += hal_i2s("\ndr6=", &buff[i], ctx->dr6, 16U, 1U);
	i += hal_i2s(" dr7=", &buff[i], ctx->dr7, 16U, 1U);

	/* clang-format off */
	__asm__ volatile (
		"movl %%cr2, %0"
	: "=r" (ss)
	:
	: );
	/* clang-format on */

	i += hal_i2s(" cr2=", &buff[i], ss, 16U, 1U);
	/* i += hal_i2s(" thr=", &buff[i], _proc_current(), 16U, 1U); */

	buff[i++] = '\n';

	buff[i] = '\0';

	return;
}


static void exceptions_defaultHandler(unsigned int n, exc_context_t *ctx)
{
	char buff[SIZE_CTXDUMP];

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

#ifdef NDEBUG
	hal_cpuReboot();
#else
	proc_crash(proc_current());
	proc_threadEnd();
#endif
}


static void exceptions_trampoline(unsigned int n, exc_context_t *ctx)
{
	exceptions.defaultHandler(n, ctx);
}


int hal_exceptionsSetHandler(unsigned int n, excHandlerFn_t handler)
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
static __attribute__((section(".init"))) void _exceptions_setIDTStub(unsigned int n, void (*addr)(void))
{
	u32 w0, w1;
	u32 *idtr;

	/* parasoft-begin-suppress MISRAC2012-RULE_11_1 "Must pass the address of exception handler to hw reg" */
	w0 = ((u32)addr & 0xffff0000U);
	w1 = ((u32)addr & 0x0000ffffU);
	/* parasoft-end-suppress MISRAC2012-RULE_11_1 */
	w0 |= IGBITS_DPL3 | IGBITS_PRES | IGBITS_SYSTEM | IGBITS_IRQEXC;
	w1 |= ((u32)SEL_KCODE << 16);

	idtr = (u32 *)syspage->hs.idtr.addr;
	idtr[n * 2U + 1U] = w0;
	idtr[n * 2U] = w1;

	return;
}


/* Function initializes exception handling */
__attribute__((section(".init"))) void _hal_exceptionsInit(void)
{
	unsigned int k;

	hal_spinlockCreate(&exceptions.lock, "exceptions.lock");
	exceptions.defaultHandler = exceptions_defaultHandler;

	_exceptions_setIDTStub(0U, _exceptions_exc0);
	_exceptions_setIDTStub(1U, _exceptions_exc1);
	_exceptions_setIDTStub(2U, _exceptions_exc2);
	_exceptions_setIDTStub(3U, _exceptions_exc3);
	_exceptions_setIDTStub(4U, _exceptions_exc4);
	_exceptions_setIDTStub(5U, _exceptions_exc5);
	_exceptions_setIDTStub(6U, _exceptions_exc6);
	_exceptions_setIDTStub(7U, _exceptions_exc7);
	_exceptions_setIDTStub(8U, _exceptions_exc8);
	_exceptions_setIDTStub(9U, _exceptions_exc9);
	_exceptions_setIDTStub(10U, _exceptions_exc10);
	_exceptions_setIDTStub(11U, _exceptions_exc11);
	_exceptions_setIDTStub(12U, _exceptions_exc12);
	_exceptions_setIDTStub(13U, _exceptions_exc13);
	_exceptions_setIDTStub(14U, _exceptions_exc14);
	_exceptions_setIDTStub(15U, _exceptions_exc15);
	_exceptions_setIDTStub(16U, _exceptions_exc16);
	_exceptions_setIDTStub(17U, _exceptions_exc17);
	_exceptions_setIDTStub(18U, _exceptions_exc18);
	_exceptions_setIDTStub(19U, _exceptions_exc19);
	_exceptions_setIDTStub(20U, _exceptions_exc20);
	_exceptions_setIDTStub(21U, _exceptions_exc21);
	_exceptions_setIDTStub(22U, _exceptions_exc22);
	_exceptions_setIDTStub(23U, _exceptions_exc23);
	_exceptions_setIDTStub(24U, _exceptions_exc24);
	_exceptions_setIDTStub(25U, _exceptions_exc25);
	_exceptions_setIDTStub(26U, _exceptions_exc26);
	_exceptions_setIDTStub(27U, _exceptions_exc27);
	_exceptions_setIDTStub(28U, _exceptions_exc28);
	_exceptions_setIDTStub(29U, _exceptions_exc29);
	_exceptions_setIDTStub(30U, _exceptions_exc30);
	_exceptions_setIDTStub(31U, _exceptions_exc31);

	for (k = 0; k < SIZE_EXCHANDLERS; k++) {
		exceptions.handlers[k] = exceptions_trampoline;
	}
	exceptions.handlers[7] = exceptions_exc7_handler;

	return;
}
