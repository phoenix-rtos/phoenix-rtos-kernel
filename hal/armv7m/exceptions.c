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

#include <board_config.h>

#include "hal/exceptions.h"
#include "hal/cpu.h"
#include "hal/console.h"
#include "hal/string.h"
#include "config.h"
#include "proc/coredump.h"
#include "proc/elf.h"

#define SIZE_FPUCTX (16 * sizeof(u32))

static struct {
	void (*handler)(unsigned int, exc_context_t *);
} hal_exception_common;


extern void hal_exceptionJump(unsigned int n, exc_context_t *ctx, void (*handler)(unsigned int, exc_context_t *));


const char *hal_exceptionMnemonic(int n)
{
	static const char *mnemonics[] = {
		"0 #InitialSP", "1 #Reset", "2 #NMI", "3 #HardFault",
		"4 #MemMgtFault", "5 #BusFault", "6 #UsageFault", "7 #",
		"8 #", "9 #", "10 #", "11 #SVC",
		"12 #Debug", "13 #", "14 #PendSV", "15 #SysTick"
	};

	return mnemonics[n & 0xf];
}


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, int n)
{
	size_t i = 0;
	u32 msp = (u32)ctx + sizeof(*ctx);
	u32 psp = ctx->psp;
	cpu_hwContext_t *hwctx;

	/* If we came from userspace HW ctx in on psp stack */
	if (ctx->irq_ret == RET_THREAD_PSP) {
		hwctx = (void *)ctx->psp;
		msp -= sizeof(cpu_hwContext_t);
		psp += sizeof(cpu_hwContext_t);
#ifdef CPU_IMXRT /* FIXME - check if FPU was enabled instead */
		psp += SIZE_FPUCTX;
#endif
	}
	else {
		hwctx = &ctx->hwctx;
#ifdef CPU_IMXRT
		msp += SIZE_FPUCTX;
#endif
	}

	hal_strcpy(buff, "\nException: ");
	hal_strcpy(buff += hal_strlen(buff), hal_exceptionMnemonic(n));
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
	i += hal_i2s(" exr=", &buff[i], ctx->irq_ret, 16, 1);
	i += hal_i2s(" bfa=", &buff[i], *(u32 *)0xe000ed38, 16, 1);

	i += hal_i2s("\ncfs=", &buff[i], *(u32 *)0xe000ed28, 16, 1);
	i += hal_i2s(" mma=", &buff[i], *(u32 *)0xe000ed34, 16, 1);

	buff[i++] = '\n';

	buff[i] = 0;
}


__attribute__((noreturn)) static void exceptions_fatal(unsigned int n, exc_context_t *ctx)
{
	char buff[SIZE_CTXDUMP];

	hal_cpuDisableInterrupts();

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

	coredump_dump(n, ctx);

#ifdef NDEBUG
	hal_cpuReboot();
#endif

	for (;;) {
		hal_cpuHalt();
	}
}


void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
{
	if ((hal_exception_common.handler != NULL) &&
			((ctx->irq_ret & (1 << 2)) != 0)) {

		/* Need to enter the kernel by returning to the
		 * thread mode. Otherwise we won't be able to
		 * enable interrupts. */
		hal_exceptionJump(n, ctx, hal_exception_common.handler);
	}

	/* Early exception, exception in kernel or proc module
	 * handler failed to kill the process and we're back
	 * here. This is a fatal error, crash the system */
	exceptions_fatal(n, ctx);
}


ptr_t hal_exceptionsPC(exc_context_t *ctx)
{
	cpu_hwContext_t *hwctx;

	if (ctx->irq_ret == RET_THREAD_PSP) {
		hwctx = (void *)ctx->psp;
	}
	else {
		hwctx = &ctx->hwctx;
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
#ifndef KERNEL_REBOOT_ON_EXCEPTION
	/* Instruction trapping TODO, handle general fault for now */
	if (n == EXC_DEFAULT) {
		hal_exception_common.handler = handler;
	}
#endif

	return 0;
}


void _hal_exceptionsInit(void)
{
	hal_exception_common.handler = NULL;
}


extern cpu_context_t *hal_excToCpuCtx(exc_context_t *ctx)
{
	return ctx;
}


void hal_coredumpGRegset(void *buff, cpu_context_t *ctx)
{
	u32 psp = ctx->psp;
	cpu_hwContext_t *hwctx;

	/* If we came from userspace HW ctx in on psp stack */
	if (ctx->irq_ret == RET_THREAD_PSP) {
		hwctx = (void *)ctx->psp;
		psp += sizeof(cpu_hwContext_t);
#ifdef CPU_IMXRT /* FIXME - check if FPU was enabled instead */
		psp += SIZE_FPUCTX;
#endif
	}
	else {
		hwctx = &ctx->hwctx;
	}

	u32 *regs = (u32 *)buff;
	*(regs++) = hwctx.r0;
	*(regs++) = hwctx.r1;
	*(regs++) = hwctx.r2;
	*(regs++) = hwctx.r3;
	*(regs++) = ctx->r4;
	*(regs++) = ctx->r5;
	*(regs++) = ctx->r6;
	*(regs++) = ctx->r7;
	*(regs++) = ctx->r8;
	*(regs++) = ctx->r9;
	*(regs++) = ctx->r10;
	*(regs++) = ctx->r11;
	*(regs++) = hwctx.r12;
	*(regs++) = psp;
	*(regs++) = hwctx.lr;
	*(regs++) = hwctx.pc;
	*(regs++) = hwctx.psr;
}


void hal_coredumpThreadAux(void *buff, cpu_context_t *ctx)
{
#if defined(CPU_IMXRT) && defined(PROC_COREDUMP_FPUCTX)
	static const char ARMVFP_NAME[] = "LINUX";
	Elf32_Nhdr nhdr;
	nhdr.n_namesz = sizeof(ARMVFP_NAME);
	nhdr.n_descsz = sizeof(u64) * 32 + sizeof(ctx->fpscr);
	nhdr.n_type = NT_ARM_VFP;
	hal_memcpy(buff, &nhdr, sizeof(nhdr));
	buff = (char *)buff + sizeof(nhdr);
	hal_memcpy(buff, ARMVFP_NAME, sizeof(ARMVFP_NAME));
	buff = (char *)buff + ((sizeof(ARMVFP_NAME) + 3) & ~3);
	hal_memcpy(buff, &ctx->s0, sizeof(u32) * 16);
	buff = (char *)buff + sizeof(u32) * 16;
	hal_memcpy(buff, &ctx->s16, sizeof(u32) * 16);
	buff = (char *)buff + sizeof(u32) * 16;
	buff = (char *)buff + sizeof(u64) * 16; /* padding to match full VFPv3 */
	hal_memcpy(buff, &ctx->fpscr, sizeof(ctx->fpscr));
#endif
}


void hal_coredumpGeneralAux(void *buff)
{
#if defined(CPU_IMXRT) && defined(PROC_COREDUMP_FPUCTX)
	static const char AUXV_NAME[] = "CORE";
	Elf32_Nhdr nhdr;
	struct {
		u32 a_type;
		u32 a_val;
	} auxv[2];

	nhdr.n_namesz = sizeof(AUXV_NAME);
	nhdr.n_descsz = sizeof(auxv);
	nhdr.n_type = NT_AUXV;
	hal_memcpy(buff, &nhdr, sizeof(nhdr));
	buff = (char *)buff + sizeof(nhdr);
	hal_memcpy(buff, AUXV_NAME, sizeof(AUXV_NAME));
	buff = (char *)buff + ((sizeof(AUXV_NAME) + 3) & ~3);

	auxv[0].a_type = AT_HWCAP;
	auxv[0].a_val = HWCAP_VFPv3D16;
	auxv[1].a_type = AT_NULL;
	auxv[1].a_val = 0;
	hal_memcpy(buff, auxv, sizeof(auxv));
#endif
}
