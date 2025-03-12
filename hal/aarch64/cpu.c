/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014, 2017, 2018, 2024 Phoenix Systems
 * Author: Jacek Popko, Aleksander Kaminski, Pawel Pisarczyk, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/cpu.h"
#include "hal/string.h"
#include "hal/spinlock.h"
#include "hal/hal.h"

#include "aarch64.h"
#include "config.h"

#define CONST_STR_SIZE(x) x, (sizeof(x) - 1)

/* Function creates new cpu context on top of given thread kernel stack */
int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg, hal_tls_t *tls)
{
	cpu_context_t *ctx;
	int i;

	(void)tls;

	*nctx = 0;
	if (kstack == NULL) {
		return -1;
	}

	kstacksz &= ~0xf;

	if (kstacksz < sizeof(cpu_context_t)) {
		return -1;
	}

	/* Align user stack to 16 bytes */
	ustack = (void *)((ptr_t)ustack & ~0xf);

	/* Prepare initial kernel stack */
	ctx = (cpu_context_t *)(kstack + kstacksz - sizeof(cpu_context_t));

	/* Set all registers to NAN */
	for (i = 0; i < 64; i += 2) {
		ctx->freg[i] = ~0uL;
		ctx->freg[i + 1] = ~0uL;
	}

	ctx->fpsr = 0;
	ctx->fpcr = 0;
	ctx->cpacr = 0;

	ctx->x[0] = (u64)arg;
	for (i = 1; i < 31; i++) {
		ctx->x[i] = 0x0101010101010101uL * i;
	}

	ctx->pc = (u64)start;

	/* Enable interrupts, set normal execution mode */
	if (ustack != NULL) {
		ctx->psr = MODE_EL0;
		ctx->sp = (u64)ustack;
	}
	else {
		ctx->psr = MODE_EL1_SP1;
		ctx->sp = (u64)kstack + kstacksz;
	}

	ctx->x[29] = ctx->sp;
	*nctx = ctx;

	return 0;
}


int hal_cpuPushSignal(void *kstack, void (*handler)(void), cpu_context_t *signalCtx, int n, const int src)
{
	cpu_context_t *ctx = (void *)((char *)kstack - sizeof(cpu_context_t));
	const struct stackArg args[] = {
		{ &ctx->psr, sizeof(ctx->psr) },
		{ &ctx->sp, sizeof(ctx->sp) },
		{ &ctx->pc, sizeof(ctx->pc) },
		{ &signalCtx, sizeof(signalCtx) },
		{ &n, sizeof(n) },
	};

	(void)src;

	hal_memcpy(signalCtx, ctx, sizeof(cpu_context_t));

	signalCtx->pc = (u64)handler;
	signalCtx->sp -= sizeof(cpu_context_t);

	hal_stackPutArgs((void **)&signalCtx->sp, sizeof(args) / sizeof(args[0]), args);

	return 0;
}


void hal_cpuSigreturn(void *kstack, void *ustack, cpu_context_t **ctx)
{
	(void)kstack;
	GETFROMSTACK(ustack, u64, (*ctx)->pc, 2);
	GETFROMSTACK(ustack, u64, (*ctx)->sp, 3);
	GETFROMSTACK(ustack, u64, (*ctx)->psr, 4);
}


static void appendToString(const char *in, size_t inLen, char *out, size_t *n, size_t limit)
{
	if ((*n + inLen) >= limit) {
		return;
	}

	hal_strcpy(&out[*n], in);
	*n += inLen;
}


void hal_cpuGetProcID(struct aarch64_proc_id *out)
{
	out->mmfr0 = sysreg_read(id_aa64mmfr0_el1);
	out->pfr0 = sysreg_read(id_aa64pfr0_el1);
	out->isar0 = sysreg_read(id_aa64isar0_el1);
	out->dfr0 = (u32)sysreg_read(id_aa64dfr0_el1);
	out->midr = (u32)sysreg_read(midr_el1);
}


char *hal_cpuInfo(char *info)
{
	size_t n = 0;
	unsigned cpuCount = hal_cpuGetCount();
	struct aarch64_proc_id procId;

	hal_cpuGetProcID(&procId);
	appendToString(CONST_STR_SIZE(HAL_NAME_PLATFORM), info, &n, 128);


	if (((procId.midr >> 16) & 0xf) == 0xf) {
		appendToString(CONST_STR_SIZE("ARMv8 "), info, &n, 128);
	}

	if (((procId.midr >> 4) & 0xfff) == 0xd03) {
		appendToString(CONST_STR_SIZE("Cortex-A53 "), info, &n, 128);
	}

	info[n++] = 'r';
	info[n++] = '0' + ((procId.midr >> 20) & 0xf);
	info[n++] = 'p';
	info[n++] = '0' + (procId.midr & 0xf);

	info[n++] = ' ';
	info[n++] = 'x';
	if (cpuCount >= 10) {
		info[n++] = '0' + (cpuCount / 10);
	}

	info[n++] = '0' + (cpuCount % 10);

	info[n] = '\0';

	return info;
}


char *hal_cpuFeatures(char *features, unsigned int len)
{
	size_t n = 0;
	struct aarch64_proc_id procId;

	hal_cpuGetProcID(&procId);
	if (len == 0) {
		return features;
	}

	if (((procId.pfr0 >> 12) & 0xf) != 0) {
		appendToString(CONST_STR_SIZE("EL3, "), features, &n, len);
	}

	if (((procId.pfr0 >> 8) & 0xf) != 0) {
		appendToString(CONST_STR_SIZE("EL2, "), features, &n, len);
	}

	switch ((procId.pfr0 >> 16) & 0xf) {
		case 0:
			appendToString(CONST_STR_SIZE("FP, "), features, &n, len);
			break;

		case 1:
			appendToString(CONST_STR_SIZE("FP16, "), features, &n, len);
			break;

		default:
			break;
	}

	switch ((procId.pfr0 >> 20) & 0xf) {
		case 0: /* Fall-through */
		case 1:
			appendToString(CONST_STR_SIZE("AdvSIMD, "), features, &n, len);
			break;

		default:
			break;
	}

	switch ((procId.isar0 >> 4) & 0xf) {
		case 1: /* Fall-through */
		case 2:
			appendToString(CONST_STR_SIZE("AES, "), features, &n, len);
			break;

		default:
			break;
	}

	switch ((procId.isar0 >> 8) & 0xf) {
		case 1:
			appendToString(CONST_STR_SIZE("SHA1, "), features, &n, len);
			break;

		default:
			break;
	}

	switch ((procId.isar0 >> 12) & 0xf) {
		case 1:
			appendToString(CONST_STR_SIZE("SHA256, "), features, &n, len);
			break;

		case 2:
			appendToString(CONST_STR_SIZE("SHA512, "), features, &n, len);
			break;

		default:
			break;
	}

	switch ((procId.isar0 >> 16) & 0xf) {
		case 1:
			appendToString(CONST_STR_SIZE("CRC32, "), features, &n, len);
			break;

		default:
			break;
	}

	switch ((procId.isar0 >> 20) & 0xf) {
		case 2: /* Fall-through */
		case 3:
			appendToString(CONST_STR_SIZE("LSE, "), features, &n, len);
			break;

		default:
			break;
	}

	if (n > 0) {
		features[n - 2] = '\0';
	}
	else {
		features[0] = '\0';
	}

	return features;
}


void hal_cpuTlsSet(hal_tls_t *tls, cpu_context_t *ctx)
{
	/* In theory there should be 16-byte thread control block but
	 * it's stored elsewhere so we need to subtract 16 from the pointer
	 */
	ptr_t ptr = tls->tls_base - 16;
	sysreg_write(tpidr_el0, ptr);
	hal_cpuDataSyncBarrier();
}


void _hal_cpuSetKernelStack(void *kstack)
{
	hal_cpuDataSyncBarrier();
	sysreg_write(tpidr_el1, kstack);
	hal_cpuDataSyncBarrier();
}


void hal_cpuGetCycles(cycles_t *cb)
{
	*cb = sysreg_read(pmccntr_el0);
}


void hal_cpuLowPower(time_t us, spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	hal_spinlockClear(spinlock, sc);
	hal_cpuHalt();
}


/* cache management */


void hal_cleanDCache(ptr_t start, size_t len)
{
	hal_cpuCleanDataCache(start, start + len);
}
