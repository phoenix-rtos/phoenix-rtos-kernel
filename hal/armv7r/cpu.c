/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014-2025 Phoenix Systems
 * Author: Jacek Popko, Aleksander Kaminski, Pawel Pisarczyk, Lukasz Leczkowski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/cpu.h"
#include "hal/string.h"
#include "hal/spinlock.h"
#include "hal/hal.h"

#include "armv7r.h"
#include "config.h"


int hal_cpuCreateContext(cpu_context_t **nctx, void (*start)(void *harg), void *kstack, size_t kstacksz, void *ustack, void *arg, hal_tls_t *tls)
{
	cpu_context_t *ctx;
	int i;

	(void)tls;

	*nctx = NULL;
	if (kstack == NULL) {
		return -1;
	}

	kstacksz &= ~0x3U;

	if (kstacksz < sizeof(cpu_context_t)) {
		return -1;
	}

	/* Align user stack to 8 bytes */
	ustack = (void *)((ptr_t)ustack & ~0x7U);

	/* Prepare initial kernel stack */
	ctx = (cpu_context_t *)(kstack + kstacksz - sizeof(cpu_context_t));

	/* Set all registers to sNAN */
	for (i = 0; i < 32; i += 2) {
		ctx->freg[i] = 0;
		ctx->freg[i + 1] = 0xfff10000UL;
	}

	ctx->fpsr = 0;

	ctx->padding = 0;

	ctx->r0 = (u32)arg;
	ctx->r1 = 0x11111111UL;
	ctx->r2 = 0x22222222UL;
	ctx->r3 = 0x33333333UL;
	ctx->r4 = 0x44444444UL;
	ctx->r5 = 0x55555555UL;
	ctx->r6 = 0x66666666UL;
	ctx->r7 = 0x77777777UL;
	ctx->r8 = 0x88888888UL;
	ctx->r9 = 0x99999999UL;
	ctx->r10 = 0xaaaaaaaaUL;

	ctx->ip = 0xccccccccUL;
	ctx->lr = 0xeeeeeeeeUL;

	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "Need to assign to processor register function address" */
	ctx->pc = (u32)start;

	/* Enable interrupts, set normal execution mode */
	if (ustack != NULL) {
		ctx->psr = MODE_USR;
		ctx->sp = (u32)ustack;
	}
	else {
		ctx->psr = MODE_SYS;
		ctx->sp = (u32)kstack + kstacksz;
	}

	/* Thumb mode? */
	if ((ctx->pc & 0x1U) != 0U) {
		ctx->psr |= 0x1U << 5;
	}

	ctx->fp = ctx->sp;
	*nctx = ctx;

	return 0;
}


int hal_cpuPushSignal(void *kstack, void (*handler)(void), cpu_context_t *signalCtx, int n, unsigned int oldmask, const int src)
{
	cpu_context_t *ctx = (void *)((char *)kstack - sizeof(cpu_context_t));
	const struct stackArg args[] = {
		{ &ctx->psr, sizeof(ctx->psr) },
		{ &ctx->sp, sizeof(ctx->sp) },
		{ &ctx->pc, sizeof(ctx->pc) },
		{ &signalCtx, sizeof(signalCtx) },
		{ &oldmask, sizeof(oldmask) },
		{ &n, sizeof(n) },
	};

	(void)src;

	hal_memcpy(signalCtx, ctx, sizeof(cpu_context_t));

	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "program counter must be set to the address of the function" */
	signalCtx->pc = (u32)handler & ~0x1U;
	signalCtx->sp -= sizeof(cpu_context_t);
	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "checking in what processor mode code must be executed" */
	if (((u32)handler & 0x1U) != 0U) {
		signalCtx->psr |= THUMB_STATE;
	}
	else {
		signalCtx->psr &= ~THUMB_STATE;
	}

	hal_stackPutArgs((void **)&signalCtx->sp, sizeof(args) / sizeof(args[0]), args);

	return 0;
}


void hal_cpuSigreturn(void *kstack, void *ustack, cpu_context_t **ctx)
{
	(void)kstack;
	GETFROMSTACK(ustack, u32, (*ctx)->pc, 2);
	GETFROMSTACK(ustack, u32, (*ctx)->sp, 3);
	GETFROMSTACK(ustack, u32, (*ctx)->psr, 4);
}


char *hal_cpuInfo(char *info)
{
	size_t n = 0;
	u32 midr;

	(void)hal_strcpy(info, HAL_NAME_PLATFORM);
	n = sizeof(HAL_NAME_PLATFORM) - 1U;

	midr = hal_cpuGetMIDR();

	if (((midr >> 16) & 0xfU) == 0xfU) {
		(void)hal_strcpy(&info[n], "ARMv7 ");
		n += 6U;
	}

	if (((midr >> 4) & 0xfffUL) == 0xc15UL) {
		(void)hal_strcpy(&info[n], "Cortex-R5 ");
		n += hal_strlen("Cortex-R5 ");
	}
	/* parasoft-begin-suppress MISRAC2012-RULE_10_2 | MISRAC2012-RULE_10_3
	 * "Correctness of char operation in following block is guaranteed by design"
	 */
	info[n++] = 'r';
	info[n++] = '0' + ((midr >> 20) & 0xfUL);
	info[n++] = 'p';
	info[n++] = '0' + (midr & 0xfUL);

	info[n++] = ' ';
	info[n++] = 'x';
	info[n++] = '0' + hal_cpuGetCount();

	info[n] = '\0';
	/* parasoft-end-suppress MISRAC2012-RULE_10_2 | MISRAC2012-RULE_10_3 */

	return info;
}


char *hal_cpuFeatures(char *features, unsigned int len)
{
	unsigned int n = 0;
	u32 pfr0 = hal_cpuGetPFR0(), pfr1 = hal_cpuGetPFR1();

	if (len == 0U) {
		return features;
	}

	if (((pfr0 >> 12) & 0xfUL) != 0U && (len - n) > 9U) {
		(void)hal_strcpy(&features[n], "ThumbEE, ");
		n += 9U;
	}

	if (((pfr0 >> 8) & 0xfUL) != 0U && (len - n) > 9U) {
		(void)hal_strcpy(&features[n], "Jazelle, ");
		n += 9U;
	}

	if (((pfr0 >> 4) & 0xfUL) != 0U && (len - n) > 7U) {
		(void)hal_strcpy(&features[n], "Thumb, ");
		n += 7U;
	}

	if ((pfr0 & 0xfUL) != 0U && (len - n) > 5U) {
		(void)hal_strcpy(&features[n], "ARM, ");
		n += 5U;
	}

	if (((pfr1 >> 16) & 0xfUL) != 0U && (len - n) > 15U) {
		(void)hal_strcpy(&features[n], "Generic Timer, ");
		n += 15U;
	}

	if (((pfr1 >> 12) & 0xfUL) != 0U && (len - n) > 16U) {
		(void)hal_strcpy(&features[n], "Virtualization, ");
		n += 16U;
	}

	if (((pfr1 >> 8) & 0xfUL) != 0U && (len - n) > 5U) {
		(void)hal_strcpy(&features[n], "MCU, ");
		n += 5U;
	}

	if (((pfr1 >> 4) & 0xfUL) != 0U && (len - n) > 10U) {
		(void)hal_strcpy(&features[n], "Security, ");
		n += 10U;
	}

	if (n > 0U) {
		features[n - 2U] = '\0';
	}
	else {
		features[0] = '\0';
	}

	return features;
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
void hal_cpuTlsSet(hal_tls_t *tls, cpu_context_t *ctx)
{
	/* In theory there should be 8-byte thread control block but
	 * it's stored elsewhere so we need to subtract 8 from the pointer
	 */
	ptr_t ptr = tls->tls_base - 8U;
	__asm__ volatile("mcr p15, 0, %[value], cr13, cr0, 3;" ::[value] "r"(ptr));
}


void hal_cpuLowPower(time_t us, spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	hal_spinlockClear(spinlock, sc);
	hal_cpuHalt();
}


void hal_cleanDCache(ptr_t start, size_t len)
{
	hal_cpuCleanDataCache(start, start + len);
}


void _hal_cpuInit(void)
{
}
