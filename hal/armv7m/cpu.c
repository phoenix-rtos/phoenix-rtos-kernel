/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2014, 2017 Phoenix Systems
 * Author: Jacek Popko, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../cpu.h"
#include "../interrupts.h"
#include "../spinlock.h"
#include "../timer.h"
#include "../string.h"


#if defined(CPU_STM32L152XD) || defined(CPU_STM32L152XE) || defined(CPU_STM32L4X6)
#include "stm32.h"
#endif

#if defined(CPU_IMXRT105X) || defined(CPU_IMXRT106X)
#include "imxrt10xx.h"
#endif

#ifdef CPU_IMXRT117X
#include "imxrt117x.h"
#endif


struct {
	int busy;
	spinlock_t busySp;
} cpu_common;


volatile cpu_context_t *_cpu_nctx;


/* interrupts */


void hal_cpuDisableInterrupts(void)
{
	__asm__ volatile ("cpsid if");
}


void hal_cpuEnableInterrupts(void)
{
	__asm__ volatile ("cpsie if");
}


/* performance */


void hal_cpuLowPower(time_t us)
{
#ifdef CPU_STM32
	spinlock_ctx_t scp;

	hal_spinlockSet(&cpu_common.busySp, &scp);
	if (cpu_common.busy == 0) {
		/* Don't increment jiffies if sleep was unsuccessful */
		us = _stm32_pwrEnterLPStop(us);
		timer_jiffiesAdd(us);
	}
	hal_spinlockClear(&cpu_common.busySp, &scp);
#endif
}


void hal_cpuSetDevBusy(int s)
{
	spinlock_ctx_t scp;

	hal_spinlockSet(&cpu_common.busySp, &scp);
	if (s == 1)
		++cpu_common.busy;
	else
		--cpu_common.busy;

	if (cpu_common.busy < 0)
		cpu_common.busy = 0;
	hal_spinlockClear(&cpu_common.busySp, &scp);
}


void hal_cpuHalt(void)
{
#ifndef CPU_IMXRT117X
	__asm__ volatile ("\
		wfi; \
		nop; ");
#endif
}


void hal_cpuGetCycles(cycles_t *cb)
{
#ifdef CPU_STM32
	*cb = _stm32_systickGet();
#elif defined(CPU_IMXRT)
	*cb = _imxrt_systickGet();
#endif
}


/* bit operations */


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


/* context management */

void hal_cpuSetCtxGot(cpu_context_t *ctx, void *got)
{
	ctx->r9 = (u32)got;
}


void hal_cpuSetGot(void *got)
{
	__asm__ volatile ("mov r9, %0" :: "r" (got));
}


void *hal_cpuGetGot(void)
{
	void *got;

	__asm__ volatile ("mov %0, r9" : "=r" (got));

	return got;
}


int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg)
{
	cpu_context_t *ctx;

	*nctx = 0;
	if (kstack == NULL)
		return -1;

	if (kstacksz < sizeof(cpu_context_t))
		return -1;

	/* Align user stack to 8 bytes */
	ustack = (void *)((ptr_t)ustack & ~0x7);

	/* Prepare initial kernel stack */
	ctx = (cpu_context_t *)(kstack + kstacksz - sizeof(cpu_context_t));

	hal_memset(ctx, 0, sizeof(*ctx));

	ctx->savesp = (u32)ctx;
	ctx->psp = (ustack != NULL) ? (u32)ustack - (HWCTXSIZE * sizeof(int)) : NULL;
	ctx->r4 = 0x44444444;
	ctx->r5 = 0x55555555;
	ctx->r6 = 0x66666666;
	ctx->r7 = 0x77777777;
	ctx->r8 = 0x88888888;
	ctx->r9 = 0x99999999;
	ctx->r10 = 0xaaaaaaaa;
	ctx->r11 = 0xbbbbbbbb;

	if (ustack != NULL) {
		((u32 *)ctx->psp)[0] = (u32)arg;   /* r0 */
		((u32 *)ctx->psp)[1] = 0x11111111; /* r1 */
		((u32 *)ctx->psp)[2] = 0x22222222; /* r2 */
		((u32 *)ctx->psp)[3] = 0x33333333; /* r3 */
		((u32 *)ctx->psp)[4] = 0xcccccccc; /* r12 */
		((u32 *)ctx->psp)[5] = 0xeeeeeeee; /* lr */
		((u32 *)ctx->psp)[6] = (u32)start; /* pc */
		((u32 *)ctx->psp)[7] = 0x01000000; /* psr */
#ifdef CPU_IMXRT
		ctx->fpuctx = ctx->psp + 8 * sizeof(int);
		((u32 *)ctx->psp)[24] = 0;         /* fpscr */
#endif
		ctx->irq_ret = RET_THREAD_PSP;
	}
	else {
		ctx->r0 = (u32)arg;
		ctx->r1 = 0x11111111;
		ctx->r2 = 0x22222222;
		ctx->r3 = 0x33333333;
		ctx->r12 = 0xcccccccc;
		ctx->lr = 0xeeeeeeee;
		ctx->pc = (u32)start;
		ctx->psr = 0x01000000;
		ctx->fpuctx = (u32)(&ctx->psr + 1);
#ifdef CPU_IMXRT
		ctx->fpscr = 0;
#endif
		ctx->irq_ret = RET_THREAD_MSP;
	}

	*nctx = ctx;
	return 0;
}


void hal_cpuRestore(cpu_context_t *curr, cpu_context_t *next)
{
	curr->savesp = (u32)next;
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


void _hal_cpuSetKernelStack(void *kstack)
{
}


void *hal_cpuGetSP(cpu_context_t *ctx)
{
	return (void *)ctx;
}


void *hal_cpuGetUserSP(cpu_context_t *ctx)
{
	return (void *)ctx->psp;
}


int hal_cpuSupervisorMode(cpu_context_t *ctx)
{
	return 0;
}


int hal_cpuPushSignal(void *kstack, void (*handler)(void), int sig)
{
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


void hal_longjmp(cpu_context_t *ctx)
{
	__asm__ volatile
	(" \
		cpsid if; \
		str %1, [%0]; \
		bl _hal_invokePendSV; \
		cpsie if; \
	1:	b 1b"
	:
	: "r" (&_cpu_nctx), "r" (ctx)
	: "memory");
}


/* core management */


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
	int i;
	unsigned int cpuinfo;

#ifdef CPU_STM32
	cpuinfo = _stm32_cpuid();
	hal_strcpy(info, "STM32 ");
	i = 6;
#elif defined(CPU_IMXRT)
	cpuinfo = _imxrt_cpuid();
	hal_strcpy(info, "i.MX RT ");
	i = 8;
#else
	hal_strcpy(info, "unknown");
	return info;
#endif
	if (((cpuinfo >> 24) & 0xff) == 0x41) {
		hal_strcpy(info + i, "ARM ");
		i += 4;
	}

	*(info + i++) = 'r';
	*(info + i++) = '0' + ((cpuinfo >> 20) & 0xf);
	*(info + i++) = ' ';

	if (((cpuinfo >> 4) & 0xfff) == 0xc23) {
		hal_strcpy(info + i, "Cortex-M3 ");
		i += 10;
	}

	*(info + i++) = 'p';
	*(info + i++) = '0' + (cpuinfo & 0xf);
	*(info + i++) = '\0';

	return info;
}


char *hal_cpuFeatures(char *features, unsigned int len)
{
	features[0] = '\0';
	return features;
}


void hal_wdgReload(void)
{
#ifdef CPU_STM32
	_stm32_wdgReload();
#elif defined(CPU_IMXRT)
	_imxrt_wdgReload();
#endif
}


void cpu_sendIPI(unsigned int cpu, unsigned int intr)
{
}


void _hal_cpuInit(void)
{
	cpu_common.busy = 0;
	_cpu_nctx = NULL;

	hal_spinlockCreate(&cpu_common.busySp, "devBusy");

#ifdef CPU_STM32
	_stm32_platformInit();
#elif defined(CPU_IMXRT)
	_imxrt_platformInit();
#endif
}
