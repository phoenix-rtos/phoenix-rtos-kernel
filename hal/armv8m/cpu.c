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

struct {
	int busy;
	spinlock_t busySp;
} cpu_common;


void _interrupts_nvicSystemReset(void);


/* performance */


void hal_cpuLowPower(time_t us, spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	hal_spinlockClear(spinlock, sc);
	hal_cpuHalt();
}


void hal_cpuGetCycles(cycles_t *cb)
{
	/* Cycle counter is not available on armv8m
	assumption that 1 cycle is 1us, so we use hal_timerGetUs() with 1ms resolution
	both cycles_t and time_t have the same size on armv8m */
	*cb = hal_timerGetUs();
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


int hal_cpuCreateContext(cpu_context_t **nctx, void (*start)(void *harg), void *kstack, size_t kstacksz, void *ustack, void *arg, hal_tls_t *tls)
{
	cpu_context_t *ctx;

	(void)tls;

	*nctx = 0;
	if (kstack == NULL) {
		return -1;
	}

	if (kstacksz < sizeof(cpu_context_t)) {
		return -1;
	}

	/* Align user stack to 8 bytes */
	ustack = (void *)((ptr_t)ustack & ~0x7u);

	/* Prepare initial kernel stack */
	ctx = (cpu_context_t *)(kstack + kstacksz - sizeof(cpu_context_t));

	hal_memset(ctx, 0, sizeof(*ctx));

	ctx->savesp_s = (u32)ctx;
	ctx->psp = (ustack != NULL) ? (u32)ustack - (HWCTXSIZE * sizeof(int)) : 0;
	ctx->msp = (ustack != NULL) ? (u32)kstack + kstacksz : (u32)&ctx->hwctx;
	ctx->r4 = 0x44444444u;
	ctx->r5 = 0x55555555u;
	ctx->r6 = 0x66666666u;
	ctx->r7 = 0x77777777u;
	ctx->r8 = 0x88888888u;
	ctx->r9 = 0x99999999u;
	ctx->r10 = 0xaaaaaaaau;
	ctx->r11 = 0xbbbbbbbbu;

	ctx->hwctx.r0 = (u32)arg;
	ctx->hwctx.r1 = 0x11111111;
	ctx->hwctx.r2 = 0x22222222;
	ctx->hwctx.r3 = 0x33333333;
	ctx->hwctx.r12 = 0xcccccccc;
	ctx->hwctx.lr = 0xeeeeeeee;
	ctx->hwctx.pc = (u32)start;
	ctx->hwctx.psr = DEFAULT_PSR;
	if (ustack != NULL) {
#if KERNEL_FPU_SUPPORT
		ctx->fpuctx = ctx->psp + (8 * sizeof(u32)); /* Must point to s0 in hw-saved context */
		ctx->fpscr = _hal_scsGetDefaultFPSCR();
#endif
		ctx->irq_ret = RET_THREAD_PSP;
	}
	else {
#if KERNEL_FPU_SUPPORT
		ctx->fpuctx = (u32)(&ctx->hwctx) + (8 * sizeof(u32)); /* Must point to s0 in hw-saved context */
		ctx->fpscr = _hal_scsGetDefaultFPSCR();
#endif
		ctx->irq_ret = RET_THREAD_MSP;
	}

	*nctx = ctx;
	return 0;
}


int hal_cpuPushSignal(void *kstack, void (*handler)(void), cpu_context_t *signalCtx, int n, unsigned int oldmask, const int src)
{
	cpu_context_t *ctx = (void *)((char *)kstack - sizeof(cpu_context_t));
	struct stackArg args[] = {
		{ &ctx->hwctx.psr, sizeof(ctx->hwctx.psr) },
		{ &ctx->psp, sizeof(ctx->psp) },
		{ &ctx->hwctx.pc, sizeof(ctx->hwctx.pc) },
		{ &signalCtx, sizeof(signalCtx) },
		{ &oldmask, sizeof(oldmask) },
		{ &n, sizeof(n) },
		{ 0, 0 } /* Reserve space for optional HWCTX */
	};
	size_t argc = (sizeof(args) / sizeof(args[0])) - 1;

	hal_memcpy(signalCtx, ctx, sizeof(cpu_context_t));

	signalCtx->psp -= sizeof(cpu_context_t);
	signalCtx->hwctx.pc = (u32)handler;

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
	int i;
	unsigned int cpuinfo = _hal_scsCpuID();

	hal_strcpy(info, HAL_NAME_PLATFORM);
	i = sizeof(HAL_NAME_PLATFORM) - 1;

	if (((cpuinfo >> 24) & 0xffu) == 0x41u) {
		hal_strcpy(info + i, "ARM ");
		i += 4;
	}

	if (((cpuinfo >> 4) & 0xfffu) == 0xd21u) {
#ifdef MCX_USE_CPU1
		hal_strcpy(info + i, "Micro Cortex-M33 ");
		i += 17;
#else
		hal_strcpy(info + i, "Cortex-M33 ");
		i += 11;
#endif
	}

	*(info + i++) = 'r';
	*(info + i++) = '0' + ((cpuinfo >> 20) & 0xfu);
	*(info + i++) = ' ';

	*(info + i++) = 'p';
	*(info + i++) = '0' + (cpuinfo & 0xfu);
	*(info + i) = '\0';

	return info;
}


char *hal_cpuFeatures(char *features, unsigned int len)
{
	unsigned int n = 0;
#if KERNEL_FPU_SUPPORT
	if ((len - n) > 5) {
		hal_strcpy(features + n, "FPU, ");
		n += 5;
	}
#else
	if ((len - n) > 8) {
		hal_strcpy(features + n, "softfp, ");
		n += 8;
	}
#endif
	/* TODO: get regions count from MPU controller */
	if ((len - n) > 8) {
		hal_strcpy(features + n, "MPU, ");
		n += 5;
	}

	if ((len - n) > 7) {
		hal_strcpy(features + n, "Thumb, ");
		n += 7;
	}

	if (n > 0) {
		features[n - 2] = '\0';
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
	*(ptr_t *)tls->arm_m_tls = tls->tls_base - 8;
}
