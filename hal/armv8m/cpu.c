/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014, 2017, 2022 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski, Damian Loewnau
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/cpu.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "hal/hal.h"
#include "hal/timer.h"

#include "config.h"

static struct {
	int busy;
	spinlock_t busySp;
} cpu_common;


/* performance */


void hal_cpuLowPower(time_t us, spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	hal_spinlockClear(spinlock, sc);
	hal_cpuHalt();
}


int hal_cpuLowPowerAvail(void)
{
	return 0;
}


void hal_cpuGetCycles(cycles_t *cb)
{
	/* Cycle counter is not available on armv8m
	assumption that 1 cycle is 1us, so we use hal_timerGetUs() with 1ms resolution
	both cycles_t and time_t have the same size on armv8m */
	*cb = (cycles_t)hal_timerGetUs();
}


void hal_cpuSetDevBusy(int s)
{
	spinlock_ctx_t scp;

	hal_spinlockSet(&cpu_common.busySp, &scp);
	if (s == 1) {
		++cpu_common.busy;
	}
	else {
		--cpu_common.busy;
	}

	if (cpu_common.busy < 0) {
		cpu_common.busy = 0;
	}
	hal_spinlockClear(&cpu_common.busySp, &scp);
}


int hal_cpuCreateContext(cpu_context_t **nctx, startFn_t start, void *kstack, size_t kstacksz, void *ustack, void *arg, hal_tls_t *tls)
{
	cpu_context_t *ctx;

	(void)tls;

	*nctx = NULL;
	if (kstack == NULL) {
		return -1;
	}

	if (kstacksz < sizeof(cpu_context_t)) {
		return -1;
	}

	/* Align user stack to 8 bytes */
	ustack = (void *)((ptr_t)ustack & ~0x7U);

	/* Prepare initial kernel stack */
	ctx = (cpu_context_t *)(kstack + kstacksz - sizeof(cpu_context_t));

	hal_memset(ctx, 0, sizeof(*ctx));

	ctx->savesp_s = (u32)ctx;
	ctx->psp = (ustack != NULL) ? (u32)ustack - (HWCTXSIZE * sizeof(int)) : 0;
	ctx->msp = (ustack != NULL) ? (u32)kstack + kstacksz : (u32)&ctx->hwctx;
	ctx->r4 = 0x44444444U;
	ctx->r5 = 0x55555555U;
	ctx->r6 = 0x66666666U;
	ctx->r7 = 0x77777777U;
	ctx->r8 = 0x88888888U;
	ctx->r9 = 0x99999999U;
	ctx->r10 = 0xaaaaaaaaU;
	ctx->r11 = 0xbbbbbbbbU;

	ctx->hwctx.r0 = (u32)arg;
	ctx->hwctx.r1 = 0x11111111U;
	ctx->hwctx.r2 = 0x22222222U;
	ctx->hwctx.r3 = 0x33333333U;
	ctx->hwctx.r12 = 0xccccccccU;
	ctx->hwctx.lr = 0xeeeeeeeeU;
	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "Need to assign function address to processor register" */
	ctx->hwctx.pc = (u32)start;
	ctx->hwctx.psr = DEFAULT_PSR;
	if (ustack != NULL) {
#if KERNEL_FPU_SUPPORT
		ctx->fpuctx = ctx->psp + (8U * sizeof(u32)); /* Must point to s0 in hw-saved context */
		ctx->fpscr = _hal_scsGetDefaultFPSCR();
#endif
		ctx->irq_ret = RET_THREAD_PSP;
	}
	else {
#if KERNEL_FPU_SUPPORT
		ctx->fpuctx = (u32)(&ctx->hwctx) + (8U * sizeof(u32)); /* Must point to s0 in hw-saved context */
		ctx->fpscr = _hal_scsGetDefaultFPSCR();
#endif
		ctx->irq_ret = RET_THREAD_MSP;
	}

	*nctx = ctx;
	return 0;
}


int hal_cpuPushSignal(void *kstack, void (*trampoline)(void), void (*handler)(int signo), cpu_context_t *signalCtx, int n, unsigned int oldmask, const int src)
{
	cpu_context_t *ctx = (void *)((char *)kstack - sizeof(cpu_context_t));
	struct stackArg args[] = {
		{ &ctx->hwctx.psr, sizeof(ctx->hwctx.psr) },
		{ &ctx->psp, sizeof(ctx->psp) },
		{ &ctx->hwctx.pc, sizeof(ctx->hwctx.pc) },
		{ &signalCtx, sizeof(signalCtx) },
		{ &oldmask, sizeof(oldmask) },
		{ &handler, sizeof(handler) },
		{ &n, sizeof(n) },
		{ 0U, 0U } /* Reserve space for optional HWCTX */
	};
	size_t argc = (sizeof(args) / sizeof(args[0])) - 1U;

	hal_memcpy(signalCtx, ctx, sizeof(cpu_context_t));

	signalCtx->psp -= sizeof(cpu_context_t);
	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "Need to assign function address to processor register" */
	signalCtx->hwctx.pc = (u32)trampoline;

	/* Set default PSR, clear potential ICI/IT flags */
	signalCtx->hwctx.psr = DEFAULT_PSR;

	if (src == SIG_SRC_SCHED) {
		/* We'll be returning through interrupt dispatcher,
		 * need to prepare context on ustack to be restored
		 */
		args[argc].argp = &signalCtx->hwctx;
		args[argc].sz = HWCTXSIZE * sizeof(int);
		argc++;
	}

	hal_stackPutArgs((void **)&signalCtx->psp, argc, args);

	return 0;
}


void hal_cpuSigreturn(void *kstack, void *ustack, cpu_context_t **ctx)
{
	cpu_context_t *kCtx = (void *)((char *)kstack - sizeof(cpu_context_t));

	GETFROMSTACK(ustack, u32, (*ctx)->hwctx.pc, 2);
	GETFROMSTACK(ustack, u32, (*ctx)->psp, 3);
	GETFROMSTACK(ustack, u32, (*ctx)->hwctx.psr, 4);
	(*ctx)->irq_ret = RET_THREAD_PSP;

	hal_memcpy(kCtx, *ctx, sizeof(cpu_context_t));

	*ctx = kCtx;
}


/* core management */


char *hal_cpuInfo(char *info)
{
	unsigned int i;
	unsigned int cpuinfo = _hal_scsCpuID();

	(void)hal_strcpy(info, HAL_NAME_PLATFORM);
	i = sizeof(HAL_NAME_PLATFORM) - 1U;

	if (((cpuinfo >> 24) & 0xffU) == 0x41U) {
		(void)hal_strcpy(info + i, "ARM ");
		i += 4U;
	}

	if (((cpuinfo >> 4) & 0xfffU) == 0xd21U) {
#ifdef MCX_USE_CPU1
		(void)hal_strcpy(info + i, "Micro Cortex-M33 ");
		i += 17U;
#else
		(void)hal_strcpy(info + i, "Cortex-M33 ");
		i += 11U;
#endif
	}

	*(info + i++) = 'r';
	*(info + i++) = '0' + ((cpuinfo >> 20) & 0xfU);
	*(info + i++) = ' ';

	*(info + i++) = 'p';
	*(info + i++) = '0' + (cpuinfo & 0xfU);
	*(info + i) = '\0';

	return info;
}


char *hal_cpuFeatures(char *features, size_t len)
{
	unsigned int n = 0;
#if KERNEL_FPU_SUPPORT
	if ((len - n) > 5U) {
		(void)hal_strcpy(features + n, "FPU, ");
		n += 5U;
	}
#else
	if ((len - n) > 8U) {
		(void)hal_strcpy(features + n, "softfp, ");
		n += 8U;
	}
#endif
	/* TODO: get regions count from MPU controller */
	if ((len - n) > 8U) {
		(void)hal_strcpy(features + n, "MPU, ");
		n += 5U;
	}

	if ((len - n) > 7U) {
		(void)hal_strcpy(features + n, "Thumb, ");
		n += 7U;
	}

	if (n > 0U) {
		features[n - 2U] = '\0';
	}
	else {
		features[0] = '\0';
	}

	return features;
}


/* TODO: add Watchdog implementation */
void hal_wdgReload(void)
{
}


void hal_cpuReboot(void)
{
	_hal_scsSystemReset();
}


void hal_cleanDCache(ptr_t start, size_t len)
{
	_hal_scsDCacheCleanAddr((void *)start, len);
}


void _hal_cpuInit(void)
{
	cpu_common.busy = 0;

	hal_spinlockCreate(&cpu_common.busySp, "devBusy");

	_hal_platformInit();
}


void hal_cpuBroadcastIPI(unsigned int intr)
{
}


void hal_cpuSmpSync(void)
{
}


/* Not safe to call if TLS is not present (tls_base mustn't be NULL) */
void hal_cpuTlsSet(hal_tls_t *tls, cpu_context_t *ctx)
{
	*(ptr_t *)tls->arm_m_tls = tls->tls_base - 8U;
}
