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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <board_config.h>

#include "hal/exceptions.h"
#include "hal/cpu.h"
#include "hal/console.h"
#include "hal/string.h"
#include "include/errno.h"
#include "config.h"

#define SIZE_FPUCTX (16U * sizeof(u32))

static struct {
	excHandlerFn_t handler;
} hal_exception_common;


/* parasoft-suppress-next-line MISRAC2012-RULE_8_6 "Definition in assembly" */
void hal_exceptionJump(unsigned int n, exc_context_t *ctx, excHandlerFn_t handler);


void hal_exceptionsDumpContext(char *buff, exc_context_t *ctx, unsigned int n)
{
	static const char *mnemonics[] = {
		"0 #InitialSP", "1 #Reset", "2 #NMI", "3 #HardFault",
		"4 #MemMgtFault", "5 #BusFault", "6 #UsageFault", "7 #",
		"8 #", "9 #", "10 #", "11 #SVC",
		"12 #Debug", "13 #", "14 #PendSV", "15 #SysTick"
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

	n &= 0xfU;

	(void)hal_strcpy(buff, "\nException: ");
	(void)hal_strcpy(buff += hal_strlen(buff), mnemonics[n]);
	(void)hal_strcpy(buff += hal_strlen(buff), "\n");
	buff += hal_strlen(buff);

	i += hal_i2s(" r0=", &buff[i], hwctx->r0, 16U, 1U);
	i += hal_i2s("  r1=", &buff[i], hwctx->r1, 16U, 1U);
	i += hal_i2s("  r2=", &buff[i], hwctx->r2, 16U, 1U);
	i += hal_i2s("  r3=", &buff[i], hwctx->r3, 16U, 1U);

	i += hal_i2s("\n r4=", &buff[i], ctx->r4, 16U, 1U);
	i += hal_i2s("  r5=", &buff[i], ctx->r5, 16U, 1U);
	i += hal_i2s("  r6=", &buff[i], ctx->r6, 16U, 1U);
	i += hal_i2s("  r7=", &buff[i], ctx->r7, 16U, 1U);

	i += hal_i2s("\n r8=", &buff[i], ctx->r8, 16U, 1U);
	i += hal_i2s("  r9=", &buff[i], ctx->r9, 16U, 1U);
	i += hal_i2s(" r10=", &buff[i], ctx->r10, 16U, 1U);
	i += hal_i2s(" r11=", &buff[i], ctx->r11, 16U, 1U);

	i += hal_i2s("\nr12=", &buff[i], hwctx->r12, 16U, 1U);
	i += hal_i2s(" psr=", &buff[i], hwctx->psr, 16U, 1U);
	i += hal_i2s("  lr=", &buff[i], hwctx->lr, 16U, 1U);
	i += hal_i2s("  pc=", &buff[i], hwctx->pc, 16U, 1U);

	i += hal_i2s("\npsp=", &buff[i], psp, 16U, 1U);
	i += hal_i2s(" msp=", &buff[i], msp, 16U, 1U);
	i += hal_i2s(" exr=", &buff[i], ctx->excret, 16U, 1U);
	i += hal_i2s(" bfa=", &buff[i], *(u32 *)0xe000ed38U, 16U, 1U);

	i += hal_i2s("\ncfs=", &buff[i], *(u32 *)0xe000ed28U, 16U, 1U);
	i += hal_i2s(" mma=", &buff[i], *(u32 *)0xe000ed34U, 16U, 1U);

	buff[i++] = '\n';

	buff[i] = '\0';
}


__attribute__((noreturn)) static void exceptions_fatal(unsigned int n, exc_context_t *ctx)
{
	char buff[SIZE_CTXDUMP];

	hal_cpuDisableInterrupts();

	hal_exceptionsDumpContext(buff, ctx, n);
	hal_consolePrint(ATTR_BOLD, buff);

#ifdef NDEBUG
	hal_cpuReboot();
#else
	for (;;) {
		hal_cpuHalt();
	}
#endif
}


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Usage in assembly" */
void exceptions_dispatch(unsigned int n, exc_context_t *ctx)
{
	if ((hal_exception_common.handler != NULL) &&
			((ctx->excret & (1U << 2)) != 0U)) {

		/* Need to enter the kernel by returning to the thread mode.
		 * Otherwise we won't be able to enable interrupts.
		 * NOTE: the handler stack runs on top of exc_context_t,
		 * instead of standard cpu_context_t (see hal_cpuGetUserCtx()). */
		hal_exceptionJump(n, ctx, hal_exception_common.handler);
	}

	/*
	 * NOTE: in case of EXC_UNDEFINED implementation, threads_setupUserReturn()
	 * has to be called before returning to userspace to avoid fault loop
	 * with installed signal handlers.
	 */

	/* Early exception, exception in kernel or proc module
	 * handler failed to kill the process and we're back
	 * here. This is a fatal error, crash the system */
	exceptions_fatal(n, ctx);
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


vm_prot_t hal_exceptionsFaultType(unsigned int n, exc_context_t *ctx)
{
	return 0;
}


void *hal_exceptionsFaultAddr(unsigned int n, exc_context_t *ctx)
{
	return NULL;
}


int hal_exceptionsSetHandler(unsigned int n, excHandlerFn_t handler)
{
#ifndef KERNEL_REBOOT_ON_EXCEPTION
	/* Instruction trapping TODO, handle general fault for now */
	if (n == EXC_DEFAULT) {
		hal_exception_common.handler = handler;
	}
#endif

	return 0;
}


/* Checks whether word is EXC_RETURN to thread mode using PSP (entry from userspace).
 * Never matches psr (IPSR == 0 in thread mode) nor zeroed pad1 - last words of cpu_context_t */
static int hal_cpuIsUserExcReturn(u32 word)
{
	return (((word & 0xffffff00U) == 0xffffff00U) && ((word & (1U << 2)) != 0U)) ? 1 : 0;
}


/* Fault from userspace - only psp, r4-r11 and EXC_RETURN were pushed on the kstack,
 * mspctx lies past the kstack top, hw-saved context is on the user stack */
static int hal_cpuGetExcUserCtx(const void *kstackTop, pmap_t *pmap, cpu_context_t *dst)
{
	const exc_context_t *exc = (const exc_context_t *)((const u8 *)kstackTop - (sizeof(exc_context_t) - sizeof(cpu_hwContext_t)));
	size_t hwsz = sizeof(cpu_hwContext_t);

	dst->psp = exc->psp;
	dst->r4 = exc->r4;
	dst->r5 = exc->r5;
	dst->r6 = exc->r6;
	dst->r7 = exc->r7;
	dst->r8 = exc->r8;
	dst->r9 = exc->r9;
	dst->r10 = exc->r10;
	dst->r11 = exc->r11;
	dst->irq_ret = exc->excret;
	dst->msp = (u32)kstackTop;

#ifdef CPU_IMXRT
	if ((exc->excret & (1U << 4)) == 0U) {
		/* Extended frame - s0-s15, fpscr, reserved word follow hwctx both on stack and in cpu_context_t.
		 * NOTE: with lazy stacking the FPU registers are written to the frame only if
		 * the fault handler executed an FPU instruction. s16-s31 aren't saved on exception. */
		hwsz += 18U * sizeof(u32);
		dst->fpuctx = exc->psp + sizeof(cpu_hwContext_t);
	}
#endif

	if (((exc->psp & 0x3U) != 0U) || (pmap_isAllowed(pmap, (const void *)exc->psp, hwsz) == 0)) {
		/* Frame not stacked (e.g. stacking fault on user stack overflow) - only the
		 * registers saved by the ISR on the kstack are known, hwctx is left zeroed */
		dst->fpuctx = 0U;
		return -EFAULT;
	}

	hal_memcpy(&dst->hwctx, (const void *)exc->psp, hwsz);
#ifdef CPU_IMXRT
	dst->pad1 = 0U;
#endif

	return 0;
}


/* Syscall entry stores only lr, pc, psr of the hw-saved frame on the kstack (all of it on IMXRT) */
static int hal_cpuGetSyscallUserRegs(pmap_t *pmap, cpu_context_t *dst)
{
	const u32 *frame = (const u32 *)dst->psp;

	/* LSB set for _syscallend */
	dst->hwctx.pc &= ~1U;
#ifndef CPU_IMXRT
	/* lost, syscall entry reuses r12-psr slots on the user stack for arguments */
	dst->hwctx.r12 = 0U;
#endif

	if (((dst->psp & 0x3U) != 0U) || (pmap_isAllowed(pmap, frame, 4U * sizeof(u32)) == 0)) {
		/* r0-r3 unavailable, left zeroed */
		return -EFAULT;
	}

	dst->hwctx.r0 = frame[0];
	dst->hwctx.r1 = frame[1];
	dst->hwctx.r2 = frame[2];
	dst->hwctx.r3 = frame[3];

	return 0;
}


int hal_cpuGetUserCtx(cpu_context_t *ctx, const void *kstackTop, pmap_t *pmap, cpu_context_t *dst)
{
	const cpu_context_t *full = (const cpu_context_t *)((const u8 *)kstackTop - sizeof(cpu_context_t));

	if (hal_cpuSupervisorMode(ctx) == 0) {
		hal_memcpy(dst, ctx, sizeof(*dst));
		return 0;
	}

	hal_memset(dst, 0, sizeof(*dst));

	/* Fault entry pushes only exc_context_t without mspctx (hw frame went to the user stack),
	 * so its last word - EXC_RETURN - lands right below the top. In cpu_context_t that word
	 * is psr/pad1, which never looks like a user EXC_RETURN, so the layouts can't be confused. */
	if (hal_cpuIsUserExcReturn(*((const u32 *)kstackTop - 1)) != 0) {
		return hal_cpuGetExcUserCtx(kstackTop, pmap, dst);
	}

	if (full->msp != (u32)kstackTop) {
		/* cpu_context_t absent or corrupted */
		return -ENODATA;
	}

	/* Syscall entry stores 0 as irq_ret. */
	if (full->irq_ret == 0U) {
		hal_memcpy(dst, full, sizeof(*dst));
		return hal_cpuGetSyscallUserRegs(pmap, dst);
	}

	if (hal_cpuIsUserExcReturn(full->irq_ret) == 0) {
		/* Interrupt entry stores EXC_RETURN as irq_ret. */
		return -ENODATA;
	}

	/* Interrupt entry from userspace copies the whole hw-saved frame to the kstack */
	hal_memcpy(dst, full, sizeof(*dst));

	return 0;
}


void _hal_exceptionsInit(void)
{
	hal_exception_common.handler = NULL;
}
