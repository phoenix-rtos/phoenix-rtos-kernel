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

#include "../../../include/errno.h"
#include "cpu.h"
#include "interrupts.h"
#include "spinlock.h"
#include "string.h"

#ifdef CPU_STM32
#include "stm32.h"
#endif

#ifdef CPU_IMXRT
#include "imxrt.h"
#endif


struct {
	int busy;
	spinlock_t busySp;
} cpu_common;


volatile cpu_context_t *_cpu_nctx;


/* context management */


int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg)
{
	cpu_context_t *ctx;

	*nctx = 0;
	if (kstack == NULL)
		return -1;

	if (kstacksz < sizeof(cpu_context_t))
		return -1;

	/* Prepare initial kernel stack */
	ctx = (cpu_context_t *)(kstack + kstacksz - sizeof(cpu_context_t));

	ctx->savesp = (u32)ctx;
	ctx->psp = (ustack != NULL) ? (u32)ustack - (8 * sizeof(int)) : NULL;
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
		ctx->irq_ret = RET_THREAD_MSP;
	}

	*nctx = ctx;
	return EOK;
}


int hal_cpuReschedule(spinlock_t *spinlock)
{
	_hal_invokePendSV();

	if (spinlock != NULL)
		hal_spinlockClear(spinlock);

	return EOK;
}


time_t hal_cpuLowPower(time_t ms)
{
#ifdef CPU_STM32
	hal_spinlockSet(&cpu_common.busySp);
	if (cpu_common.busy == 0) {
		if ((ms << 1) > 0xffff)
			ms = 0x7fff;

		_stm32_rtcSetAlarm(ms);

		/* Don't increment jiffies if sleep was unsuccessful */
		if (!_stm32_pwrEnterLPStop())
			ms = 0;

		hal_spinlockClear(&cpu_common.busySp);
		return ms;
	}
	else {
		hal_spinlockClear(&cpu_common.busySp);
		return 0;
	}
#else
	/* TODO - low power for imxrt */
	return 0;
#endif
}


void hal_cpuSetDevBusy(int s)
{
	hal_spinlockSet(&cpu_common.busySp);
	if (s == 1)
		++cpu_common.busy;
	else
		--cpu_common.busy;

	if (cpu_common.busy < 0)
		cpu_common.busy = 0;
	hal_spinlockClear(&cpu_common.busySp);
}


void hal_cpuGetCycles(cycles_t *cb)
{
#ifdef CPU_STM32
	*cb = _stm32_systickGet();
#elif CPU_IMXRT
	*cb = _imxrt_systickGet();
#endif
}


void hal_cpuRestart(void)
{
#ifdef CPU_STM32
	_stm32_nvicSystemReset();
#elif CPU_IMXRT
	_imxrt_nvicSystemReset();
#endif
}


char *hal_cpuInfo(char *info)
{
	int i;
	unsigned int cpuinfo;

#ifdef CPU_STM32
	cpuinfo = _stm32_cpuid();
	hal_strcpy(info, "STM32 ");
	i = 6;
#elif CPU_IMXRT
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
#elif CPU_IMXRT
	_imxrt_wdgReload();
#endif
}


void _hal_cpuInit(void)
{
	cpu_common.busy = 0;
	_cpu_nctx = NULL;

	hal_spinlockCreate(&cpu_common.busySp, "devBusy");

#ifdef CPU_STM32
	_stm32_platformInit();
#endif
}
