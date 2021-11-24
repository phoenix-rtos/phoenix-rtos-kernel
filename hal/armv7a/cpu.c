/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014, 2017, 2018 Phoenix Systems
 * Author: Jacek Popko, Aleksander Kaminski, Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "armv7a.h"
#include "config.h"

#include "../cpu.h"
#include "../string.h"
#include "../spinlock.h"

void hal_cpuDisableInterrupts(void)
{
	__asm__ volatile ("cpsid if");
}


void hal_cpuEnableInterrupts(void)
{
	__asm__ volatile ("cpsie aif");
}


void hal_cpuLowPower(time_t us)
{
}


void hal_cpuSetDevBusy(int s)
{
}


void hal_cpuHalt(void)
{
	__asm__ volatile ("wfi");
}


unsigned int hal_cpuGetLastBit(const u32 v)
{
	int pos;

	__asm__ volatile ("clz %0, %1" : "=r" (pos) : "r" (v));

	return 31 - pos;
}


unsigned int hal_cpuGetFirstBit(const u32 v)
{
	unsigned pos;

	__asm__ volatile ("\
		rbit %0, %1; \
		clz  %0, %0;" : "=r" (pos) : "r" (v));

	return pos;
}


void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got)
{
}


void hal_cpuSetGot(void *got)
{
}


void *hal_cpuGetGot(void)
{
	return NULL;
}


/* Function creates new cpu context on top of given thread kernel stack */
int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg)
{
	cpu_context_t *ctx;
	int i;

	*nctx = 0;
	if (kstack == NULL)
		return -1;

	if (kstacksz < sizeof(cpu_context_t))
		return -1;

	kstacksz &= ~0x3;

	/* Prepare initial kernel stack */
	ctx = (cpu_context_t *)(kstack + kstacksz - sizeof(cpu_context_t));

	/* Set all registers to sNAN */
	for (i = 0; i < 64; i += 2) {
		ctx->freg[i] = 0;
		ctx->freg[i + 1] = 0xfff10000;
	}

	ctx->fpsr = 0;

	ctx->padding = 0;

	ctx->r0 = (u32)arg;
	ctx->r1 = 0x11111111;
	ctx->r2 = 0x22222222;
	ctx->r3 = 0x33333333;
	ctx->r4 = 0x44444444;
	ctx->r5 = 0x55555555;
	ctx->r6 = 0x66666666;
	ctx->r7 = 0x77777777;
	ctx->r8 = 0x88888888;
	ctx->r9 = 0x99999999;
	ctx->r10 = 0xaaaaaaaa;

	ctx->ip = 0xcccccccc;
	ctx->lr = 0xeeeeeeee;

	ctx->pc = (u32)start;

	/* Enable interrupts, set normal execution mode */
	if (ustack != NULL) {
		ctx->psr = USR_MODE;
		ctx->sp	 = (u32)ustack;
	}
	else {
		ctx->psr = SYS_MODE;
		ctx->sp	 = (u32)kstack + kstacksz;
	}

	/* Thumb mode? */
	if (ctx->pc & 1)
		ctx->psr |= 1 << 5;

	ctx->fp	 = ctx->sp;
	*nctx = ctx;

	return 0;
}


void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->savesp = (u32)next /*+ sizeof(u32)*/;
}


void hal_cpuSetReturnValue(cpu_context_t *ctx, int retval)
{
	ctx->r0 = retval;
}


u32 hal_cpuGetPC(void)
{
	void *pc;

	__asm__ volatile ("mov %0, pc" : "=r" (pc));

	return (u32)pc;
}


void *hal_cpuGetSP(cpu_context_t *ctx)
{
	return (void *)ctx;
}


void *hal_cpuGetUserSP(cpu_context_t *ctx)
{
	return (void *)ctx->sp;
}


int hal_cpuSupervisorMode(cpu_context_t *ctx)
{
	return ctx->psr & 0xf;
}


int hal_cpuPushSignal(void *kstack, void (*handler)(void), int n)
{
	cpu_context_t *ctx = (void *)((char *)kstack - sizeof(cpu_context_t));

	/* No signal handling inside IT block */
	if (ctx->psr & 0x600fc00)
		return -1;

	PUTONSTACK(ctx->sp, u32, ctx->pc | !!(ctx->psr & THUMB_STATE));
	PUTONSTACK(ctx->sp, int, n);

	ctx->pc = (u32)handler & ~1;

	if ((u32)handler & 1)
		ctx->psr |= THUMB_STATE;
	else
		ctx->psr &= ~THUMB_STATE;

	return 0;
}


void hal_cpuDataMemoryBarrier(void)
{
	__asm__ volatile ("dmb");
}


void hal_cpuDataSyncBarrier(void)
{
	__asm__ volatile ("dsb");
}


void hal_cpuInstrBarrier(void)
{
	__asm__ volatile ("isb");
}


unsigned int hal_cpuGetID(void)
{
	return 0;
}


unsigned int hal_cpuGetCount(void)
{
	return 1;
}


char *hal_cpuInfo(char *info)
{
	size_t n = 0;
	u32 midr;

	hal_strcpy(info, HAL_NAME_PLATFORM);
	n = sizeof(HAL_NAME_PLATFORM) - 1;

	midr = hal_cpuGetMIDR();

	if (((midr >> 16) & 0xf) == 0xf) {
		hal_strcpy(&info[n], "ARMv7 ");
		n += 6;
	}

	if (((midr >> 4) & 0xfff) == 0xc07) {
		hal_strcpy(&info[n], "Cortex-A7 ");
		n += 10;
	}
	else if (((midr >> 4) & 0xfff) == 0xc09) {
		hal_strcpy(&info[n], "Cortex-A9 ");
		n += 10;
	}

	info[n++] = 'r';
	info[n++] = '0' + ((midr >> 20) & 0xf);
	info[n++] = 'p';
	info[n++] = '0' + (midr & 0xf);

	info[n] = '\0';

	return info;
}


char *hal_cpuFeatures(char *features, unsigned int len)
{
	unsigned int n = 0;
	u32 pfr0 = hal_cpuGetPFR0(), pfr1 = hal_cpuGetPFR1();

	if (!len)
		return features;

	if ((pfr0 >> 12) & 0xf && len - n > 9) {
		hal_strcpy(&features[n], "ThumbEE, ");
		n += 9;
	}

	if ((pfr0 >> 8) & 0xf && len - n > 9) {
		hal_strcpy(&features[n], "Jazelle, ");
		n += 9;
	}

	if ((pfr0 >> 4) & 0xf && len - n > 7) {
		hal_strcpy(&features[n], "Thumb, ");
		n += 7;
	}

	if (pfr0 & 0xf && len - n > 5) {
		hal_strcpy(&features[n], "ARM, ");
		n += 5;
	}

	if ((pfr1 >> 16) & 0xf && len - n > 15) {
		hal_strcpy(&features[n], "Generic Timer, ");
		n += 15;
	}

	if ((pfr1 >> 12) & 0xf && len - n > 16) {
		hal_strcpy(&features[n], "Virtualization, ");
		n += 16;
	}

	if ((pfr1 >> 8) & 0xf && len - n > 5) {
		hal_strcpy(&features[n], "MCU, ");
		n += 5;
	}

	if ((pfr1 >> 4) & 0xf && len - n > 10) {
		hal_strcpy(&features[n], "Security, ");
		n += 10;
	}

	if (n > 0)
		features[n - 2] = '\0';
	else
		features[0] = '\0';

	return features;
}


void cpu_sendIPI(unsigned int cpu, unsigned int intr)
{
}
