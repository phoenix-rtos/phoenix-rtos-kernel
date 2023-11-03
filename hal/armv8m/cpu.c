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


volatile cpu_context_t *_cpu_nctx;


extern void _interrupts_nvicSystemReset(void);


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

int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg, hal_tls_t *tls)
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
	ctx->psp = (ustack != NULL) ? (u32)ustack - (HWCTXSIZE * sizeof(int)) : NULL;
	ctx->r4 = 0x44444444u;
	ctx->r5 = 0x55555555u;
	ctx->r6 = 0x66666666u;
	ctx->r7 = 0x77777777u;
	ctx->r8 = 0x88888888u;
	ctx->r9 = 0x99999999u;
	ctx->r10 = 0xaaaaaaaau;
	ctx->r11 = 0xbbbbbbbbu;

	if (ustack != NULL) {
		((cpu_hwContext_t *)ctx->psp)->r0 = (u32)arg;
		((cpu_hwContext_t *)ctx->psp)->r1 = 0x11111111;
		((cpu_hwContext_t *)ctx->psp)->r2 = 0x22222222;
		((cpu_hwContext_t *)ctx->psp)->r3 = 0x33333333;
		((cpu_hwContext_t *)ctx->psp)->r12 = 0xcccccccc;
		((cpu_hwContext_t *)ctx->psp)->lr = 0xeeeeeeee;
		((cpu_hwContext_t *)ctx->psp)->pc = (u32)start;
		((cpu_hwContext_t *)ctx->psp)->psr = 0x01000000;
		ctx->irq_ret = RET_THREAD_PSP;
	}
	else {
		ctx->hwctx.r0 = (u32)arg;
		ctx->hwctx.r1 = 0x11111111u;
		ctx->hwctx.r2 = 0x22222222u;
		ctx->hwctx.r3 = 0x33333333u;
		ctx->hwctx.r12 = 0xccccccccu;
		ctx->hwctx.lr = 0xeeeeeeeeu;
		ctx->hwctx.pc = (u32)start;
		ctx->hwctx.psr = 0x01000000u;

		ctx->irq_ret = RET_THREAD_MSP;
	}

	*nctx = ctx;
	return 0;
}


void hal_longjmp(cpu_context_t *ctx)
{
	/* clang-format off */
	__asm__ volatile(" \
		cpsid if; \
		str %1, [%0]; \
		bl _hal_invokePendSV; \
		cpsie if; \
	1:	b 1b"
	:
	: "r" (&_cpu_nctx), "r" (ctx)
	: "memory");
	/* clang-format on */
}


/* core management */


char *hal_cpuInfo(char *info)
{
	int i;
	unsigned int cpuinfo;

#ifdef CPU_NRF91
	cpuinfo = _nrf91_cpuid();
#else
	hal_strcpy(info, "unknown");
	return info;
#endif

	hal_strcpy(info, HAL_NAME_PLATFORM);
	i = sizeof(HAL_NAME_PLATFORM) - 1;

	if (((cpuinfo >> 24) & 0xffu) == 0x41u) {
		hal_strcpy(info + i, "ARM ");
		i += 4;
	}

	if (((cpuinfo >> 4) & 0xfffu) == 0xd21u) {
		hal_strcpy(info + i, "Cortex-M33 ");
		i += 11;
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
#ifdef CPU_NRF91
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
	_interrupts_nvicSystemReset();
}


/* TODO: add implementation */
void hal_cleanDCache(ptr_t start, size_t len)
{
}


void _hal_cpuInit(void)
{
	cpu_common.busy = 0;
	_cpu_nctx = NULL;

	hal_spinlockCreate(&cpu_common.busySp, "devBusy");

	_hal_platformInit();
}


void hal_cpuBroadcastIPI(unsigned int intr)
{
}


/* Not safe to call if TLS is not present (tls_base mustn't be NULL) */
void hal_cpuTlsSet(hal_tls_t *tls, cpu_context_t *ctx)
{
	*(ptr_t *)tls->arm_m_tls = tls->tls_base - 8;
}
