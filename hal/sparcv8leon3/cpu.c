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

#include "../hal.h"
#include "../cpu.h"
#include "../interrupts.h"
#include "../spinlock.h"
#include "../string.h"

#include "config.h"


#define STR(x)  #x
#define XSTR(x) STR(x)


int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg)
{
	cpu_context_t *ctx;
	*nctx = NULL;
	if (kstack == NULL) {
		return -1;
	}
	if (kstacksz < sizeof(cpu_context_t)) {
		return -1;
	}

	/* Align user stack to 8 bytes, SPARC requires 96 bytes always free on stack */
	if (ustack != NULL) {
		ustack = (void *)(((ptr_t)ustack & ~0x7) - 0x60);
	}

	/* Prepare initial kernel stack */
	ctx = (cpu_context_t *)(kstack + kstacksz - sizeof(cpu_context_t) - 0x60);

	hal_memset(ctx, 0, sizeof(cpu_context_t));

	ctx->o0 = (u32)arg;
	ctx->o1 = 0xf1111111;
	ctx->o2 = 0xf2222222;
	ctx->o3 = 0xf3333333;
	ctx->o4 = 0xf4444444;
	ctx->o5 = 0xf5555555;
	ctx->o7 = 0xf7777777;

	ctx->l0 = 0xeeeeeee0;
	ctx->l1 = 0xeeeeeee1;
	ctx->l2 = 0xeeeeeee2;
	ctx->l3 = 0xeeeeeee3;
	ctx->l4 = 0xeeeeeee4;
	ctx->l5 = 0xeeeeeee5;
	ctx->l6 = 0xeeeeeee6;
	ctx->l7 = 0xeeeeeee7;

	ctx->i0 = (u32)arg;
	ctx->i1 = 0x10000001;
	ctx->i2 = 0x10000002;
	ctx->i3 = 0x10000003;
	ctx->i4 = 0x10000004;
	ctx->i5 = 0x10000005;
	ctx->i6 = 0;
	ctx->i7 = (u32)start - 8;

	ctx->g1 = 0x11111111;
	ctx->g2 = 0x22222222;
	ctx->g3 = 0x33333333;
	ctx->g4 = 0x44444444;
	ctx->g5 = 0x55555555;
	ctx->g6 = 0x66666666;
	ctx->g7 = 0x77777777;

	if (ustack != NULL) {
		ctx->sp = (u32)ustack;
		/* TODO: correct PSR settings when register windows are used */
		ctx->psr = (PSR_S | PSR_ET | PSR_PS) & (~PSR_CWP);
	}
	else {
		ctx->sp = (u32)kstack + kstacksz - 0x60;
		/* supervisor mode, enable traps, cwp = 0 */
		ctx->psr = (PSR_S | PSR_ET | PSR_PS) & (~PSR_CWP);
	}

	ctx->pc = (u32)start;
	ctx->npc = (u32)start + 4;
	ctx->y = 0;

	*nctx = ctx;

	return 0;
}


int hal_cpuPushSignal(void *kstack, void (*handler)(void), int n)
{
	return 0;
}


char *hal_cpuInfo(char *info)
{
	hal_strcpy(info, HAL_NAME_PLATFORM);
	return info;
}


char *hal_cpuFeatures(char *features, unsigned int len)
{
	unsigned int n = 0;

	if ((len - n) > 12) {
		hal_strcpy(features, "GRFPU-Lite, ");
		n += 12;
	}
	if ((len - n) > 10 + hal_strlen(XSTR(NWINDOWS))) {
		hal_strcpy(features + n, XSTR(NWINDOWS) " windows, ");
		n += 10 + hal_strlen(XSTR(NWINDOWS));
	}
	if (n > 0) {
		features[n - 2] = '\0';
	}
	else {
		features[0] = '\0';
	}

	return features;
}


void hal_cleanDCache(ptr_t start, size_t len)
{
}


void hal_cpuLowPower(time_t us, spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	hal_spinlockClear(spinlock, sc);
	hal_cpuHalt();
}


void _hal_cpuInit(void)
{
	return;
}


unsigned int hal_cpuGetLastBit(unsigned long v)
{
	int lb = 31;

	if (!(v & 0xffff0000)) {
		lb -= 16;
		v = (v << 16);
	}

	if (!(v & 0xff000000)) {
		lb -= 8;
		v = (v << 8);
	}

	if (!(v & 0xf0000000)) {
		lb -= 4;
		v = (v << 4);
	}

	if (!(v & 0xc0000000)) {
		lb -= 2;
		v = (v << 2);
	}

	if (!(v & 0x80000000))
		lb -= 1;

	return lb;
}


unsigned int hal_cpuGetFirstBit(unsigned long v)
{
	int fb = 0;

	if (!(v & 0xffff)) {
		fb += 16;
		v = (v >> 16);
	}

	if (!(v & 0xff)) {
		fb += 8;
		v = (v >> 8);
	}

	if (!(v & 0xf)) {
		fb += 4;
		v = (v >> 4);
	}

	if (!(v & 0x3)) {
		fb += 2;
		v = (v >> 2);
	}

	if (!(v & 0x01))
		fb += 1;

	return fb;
}


void hal_cpuTlsSet(hal_tls_t *tls, cpu_context_t *ctx)
{
	/* thread register (g7) points to the end of TLS */
	ctx->g7 = tls->tls_base + tls->tbss_sz + tls->tdata_sz;
}
