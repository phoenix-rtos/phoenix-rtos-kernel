/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2022 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/hal.h"
#include "hal/cpu.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "hal/sparcv8leon/sparcv8leon.h"


#define ASR17_FPU_MSK (3UL << 10)

#define STR(x)  #x
#define XSTR(x) STR(x)


/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Definition in assembly" */
ptr_t hal_cpuKernelStack[NUM_CPUS];


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static const char *hal_cpuGetFpuOption(void)
{
	u32 asr;

	/* clang-format off */
	__asm__ volatile("rd %%asr17, %0" : "=r"(asr));
	/* clang-format on */

	switch ((asr & ASR17_FPU_MSK) >> 10) {
		case 0x0:
			return "No FPU";

		case 0x1:
			return "GRFPU";

		case 0x2:
			return "Meiko FPU";

		case 0x3:
			return "GRFPU-Lite";

		default:
			return "Unknown";
	}
}


int hal_cpuCreateContext(cpu_context_t **nctx, startFn_t start, void *kstack, size_t kstacksz, void *ustack, void *arg, hal_tls_t *tls)
{
	cpu_context_t *ctx;
	cpu_winContext_t *wctx;

	*nctx = NULL;

	if (kstack == NULL) {
		return -1;
	}
	if (kstacksz < sizeof(cpu_context_t)) {
		return -1;
	}

	if (ustack != NULL) {
		/* Align user stack to 8 bytes */
		ustack = (void *)((ptr_t)ustack & ~0x7U);
		ctx = (cpu_context_t *)((ptr_t)kstack + kstacksz - sizeof(cpu_context_t));
		wctx = (cpu_winContext_t *)((ptr_t)ustack - sizeof(cpu_winContext_t));

		hal_memset(ctx, 0, sizeof(cpu_context_t));
		hal_memset(wctx, 0, sizeof(cpu_winContext_t));

		wctx->fp = (ptr_t)ustack;
		ctx->psr = (PSR_S | PSR_ET) & (~PSR_CWP);
		ctx->g7 = tls->tls_base + tls->tbss_sz + tls->tdata_sz;
	}
	else {
		ctx = (cpu_context_t *)((ptr_t)kstack + kstacksz - sizeof(cpu_context_t) - sizeof(cpu_winContext_t));
		wctx = (cpu_winContext_t *)((ptr_t)ctx + sizeof(cpu_context_t));

		hal_memset(ctx, 0, (sizeof(cpu_context_t) + sizeof(cpu_winContext_t)));

		wctx->fp = (ptr_t)kstack + kstacksz;
		/* supervisor mode, enable traps, cwp = 0 */
		ctx->psr = (PSR_S | PSR_ET | PSR_PS) & (~PSR_CWP);
		ctx->g7 = 0x77777777;
	}

	ctx->o0 = (u32)arg;
	ctx->o1 = 0xf1111111U;
	ctx->o2 = 0xf2222222U;
	ctx->o3 = 0xf3333333U;
	ctx->o4 = 0xf4444444U;
	ctx->o5 = 0xf5555555U;
	ctx->o7 = 0xf7777777U;

	wctx->l0 = 0xeeeeeee0U;
	wctx->l1 = 0xeeeeeee1U;
	wctx->l2 = 0xeeeeeee2U;
	wctx->l3 = 0xeeeeeee3U;
	wctx->l4 = 0xeeeeeee4U;
	wctx->l5 = 0xeeeeeee5U;
	wctx->l6 = 0xeeeeeee6U;
	wctx->l7 = 0xeeeeeee7U;

	wctx->i0 = 0x10000000U;
	wctx->i1 = 0x10000001U;
	wctx->i2 = 0x10000002U;
	wctx->i3 = 0x10000003U;
	wctx->i4 = 0x10000004U;
	wctx->i5 = 0x10000005U;
	/* parasoft-begin-suppress MISRAC2012-RULE_11_1 "Need to assign function address to processor register" */
	wctx->i7 = (u32)start - 8U;

	ctx->g1 = 0x11111111U;
	ctx->g2 = 0x22222222U;
	ctx->g3 = 0x33333333U;
	ctx->g4 = 0x44444444U;
	ctx->g5 = 0x55555555U;
	ctx->g6 = 0x66666666U;

	ctx->sp = (u32)wctx;
	ctx->savesp = (u32)ctx;

	ctx->pc = (u32)start;
	ctx->npc = (u32)start + 4U;
	/* parasoft-end-suppress MISRAC2012-RULE_11_1 */
	ctx->y = 0;

	*nctx = ctx;

	return 0;
}


void _hal_cpuSetKernelStack(void *kstack)
{
	hal_cpuKernelStack[hal_cpuGetID()] = (ptr_t)kstack;
}


int hal_cpuPushSignal(void *kstack, void (*handler)(void), cpu_context_t *signalCtx, int n, unsigned int oldmask, const int src)
{
	cpu_context_t *ctx = (void *)((char *)kstack - sizeof(cpu_context_t));
	const struct stackArg args[] = {
		{ &ctx->psr, sizeof(ctx->psr) },
		{ &ctx->sp, sizeof(ctx->sp) },
		{ &ctx->npc, sizeof(ctx->npc) },
		{ &ctx->pc, sizeof(ctx->pc) },
		{ &signalCtx, sizeof(signalCtx) },
		{ &oldmask, sizeof(oldmask) },
		{ &n, sizeof(n) },
	};
	hal_memcpy(signalCtx, ctx, sizeof(cpu_context_t));

	/* parasoft-begin-suppress MISRAC2012-RULE_11_1 "Need to assign function address to processor register" */
	signalCtx->pc = (u32)handler;
	signalCtx->npc = (u32)handler + 4U;
	/* parasoft-end-suppress MISRAC2012-RULE_11_1 */
	signalCtx->sp -= sizeof(cpu_context_t);

	hal_stackPutArgs((void **)&signalCtx->sp, sizeof(args) / sizeof(args[0]), args);

	if (src == SIG_SRC_SCHED) {
		/* We'll be returning through interrupt dispatcher,
		 * SPARC requires always 96 bytes free on stack
		 */
		signalCtx->sp -= 0x60U;
	}

	return 0;
}


void hal_cpuSigreturn(void *kstack, void *ustack, cpu_context_t **ctx)
{
	(void)kstack;
	GETFROMSTACK(ustack, u32, (*ctx)->pc, 2);
	GETFROMSTACK(ustack, u32, (*ctx)->npc, 3);
	GETFROMSTACK(ustack, u32, (*ctx)->sp, 4);
	GETFROMSTACK(ustack, u32, (*ctx)->psr, 5);
	(*ctx)->psr &= ~PSR_PS;
	(*ctx)->psr |= PSR_ET;
}


char *hal_cpuInfo(char *info)
{
	(void)hal_strcpy(info, HAL_NAME_PLATFORM);
	return info;
}


char *hal_cpuFeatures(char *features, size_t len)
{
	unsigned int n = 0;
	const char *fpu = hal_cpuGetFpuOption();

	if ((len - n) > 12U) {
		(void)hal_strcpy(features, fpu);
		n += hal_strlen(fpu);
		(void)hal_strcpy(features + n, ", ");
		n += 2U;
	}
	if ((len - n) > 10U + hal_strlen(XSTR(NWINDOWS))) {
		(void)hal_strcpy(features + n, XSTR(NWINDOWS) " windows, ");
		n += 10U + hal_strlen(XSTR(NWINDOWS));
	}
	if (n > 0U) {
		features[n - 2U] = '\0';
	}
	else {
		features[0] = '\0';
	}

	return features;
}


void hal_cleanDCache(ptr_t start, size_t len)
{
	(void)start;
	(void)len;
}


void hal_cpuLowPower(time_t us, spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	hal_spinlockClear(spinlock, sc);
	hal_cpuHalt();
}


int hal_cpuLowPowerAvail(void)
{
	return 0;
}


unsigned int hal_cpuGetLastBit(unsigned long v)
{
	unsigned int lb = 31U;

	if ((v & 0xffff0000U) == 0U) {
		lb -= 16U;
		v = (v << 16);
	}

	if ((v & 0xff000000U) == 0U) {
		lb -= 8U;
		v = (v << 8);
	}

	if ((v & 0xf0000000U) == 0U) {
		lb -= 4U;
		v = (v << 4);
	}

	if ((v & 0xc0000000U) == 0U) {
		lb -= 2U;
		v = (v << 2);
	}

	if ((v & 0x80000000U) == 0U) {
		lb -= 1U;
	}

	return lb;
}


unsigned int hal_cpuGetFirstBit(unsigned long v)
{
	unsigned int fb = 0;

	if ((v & 0xffffU) == 0U) {
		fb += 16U;
		v = (v >> 16);
	}

	if ((v & 0xffU) == 0U) {
		fb += 8U;
		v = (v >> 8);
	}

	if ((v & 0xfU) == 0U) {
		fb += 4U;
		v = (v >> 4);
	}

	if ((v & 0x3U) == 0U) {
		fb += 2U;
		v = (v >> 2);
	}

	if ((v & 0x01U) == 0U) {
		fb += 1U;
	}

	return fb;
}


void hal_cpuTlsSet(hal_tls_t *tls, cpu_context_t *ctx)
{
	__asm__ volatile("mov %0, %%g7" ::"r"(tls->tls_base + tls->tbss_sz + tls->tdata_sz));
}


void hal_cpuSmpSync(void)
{
}
