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

	hal_memset(ctx, 0, sizeof(*ctx));

	ctx->savesp = (u32)ctx;
	ctx->psp = (ustack != NULL) ? (u32)ustack - ((8 + 18) * sizeof(int)) : NULL;
	ctx->r4 = 0x44444444;
	ctx->r5 = 0x55555555;
	ctx->r6 = 0x66666666;
	ctx->r7 = 0x77777777;
	ctx->r8 = 0x88888888;
	ctx->r9 = 0x99999999;
	ctx->r10 = 0xaaaaaaaa;
	ctx->r11 = 0xbbbbbbbb;

	ctx->s16 = 0x55aa550a;
	ctx->s17 = 0x55aa551a;
	ctx->s18 = 0x55aa552a;
	ctx->s19 = 0x55aa553a;
	ctx->s20 = 0x55aa554a;
	ctx->s21 = 0x55aa555a;
	ctx->s22 = 0x55aa556a;
	ctx->s23 = 0x55aa557a;
	ctx->s24 = 0x55aa558a;
	ctx->s25 = 0x55aa559a;
	ctx->s26 = 0x55aa55aa;
	ctx->s27 = 0x55aa55ba;
	ctx->s28 = 0x55aa55ca;
	ctx->s29 = 0x55aa55da;
	ctx->s30 = 0x55aa55ea;
	ctx->s31 = 0x55aa55fa;

	if (ustack != NULL) {
		((u32 *)ctx->psp)[0] = (u32)arg;   /* r0 */
		((u32 *)ctx->psp)[1] = 0x11111111; /* r1 */
		((u32 *)ctx->psp)[2] = 0x22222222; /* r2 */
		((u32 *)ctx->psp)[3] = 0x33333333; /* r3 */
		((u32 *)ctx->psp)[4] = 0xcccccccc; /* r12 */
		((u32 *)ctx->psp)[5] = 0xeeeeeeee; /* lr */
		((u32 *)ctx->psp)[6] = (u32)start; /* pc */
		((u32 *)ctx->psp)[7] = 0x01000000; /* psr */
		((u32 *)ctx->psp)[8] = 0xaa55aa55;
		((u32 *)ctx->psp)[9] = 0xaa55aa55;

		((u32 *)ctx->psp)[10] = 0xaa55aa50;
		((u32 *)ctx->psp)[11] = 0xaa55aa51;
		((u32 *)ctx->psp)[12] = 0xaa55aa52;
		((u32 *)ctx->psp)[13] = 0xaa55aa53;
		((u32 *)ctx->psp)[14] = 0xaa55aa54;
		((u32 *)ctx->psp)[15] = 0xaa55aa55;
		((u32 *)ctx->psp)[16] = 0xaa55aa56;
		((u32 *)ctx->psp)[17] = 0xaa55aa57;
		((u32 *)ctx->psp)[18] = 0xaa55aa58;
		((u32 *)ctx->psp)[19] = 0xaa55aa59;
		((u32 *)ctx->psp)[20] = 0xaa55aa5a;
		((u32 *)ctx->psp)[21] = 0xaa55aa5b;
		((u32 *)ctx->psp)[22] = 0xaa55aa5c;
		((u32 *)ctx->psp)[23] = 0xaa55aa5d;
		((u32 *)ctx->psp)[24] = 0xaa55aa5e;
		((u32 *)ctx->psp)[25] = 0xaa55aa5f;
		((u32 *)ctx->psp)[24] = 0xaa55aa60;
		((u32 *)ctx->psp)[25] = 0;
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

		ctx->s0 = 0x55aa550a;
		ctx->s1 = 0x55aa551a;
		ctx->s2 = 0x55aa552a;
		ctx->s3 = 0x55aa553a;
		ctx->s4 = 0x55aa554a;
		ctx->s5 = 0x55aa555a;
		ctx->s6 = 0x55aa556a;
		ctx->s7 = 0x55aa557a;
		ctx->s8 = 0x55aa558a;
		ctx->s9 = 0x55aa559a;
		ctx->s10 = 0x55aa55aa;
		ctx->s11 = 0x55aa55ba;
		ctx->s12 = 0x55aa55ca;
		ctx->s13 = 0x55aa55da;
		ctx->s14 = 0x55aa55ea;
		ctx->s15 = 0x55aa55fa;
		ctx->fpscr = 0;
		ctx->pad1 = 0x55aa55f1;
	}

	*nctx = ctx;
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
#elif CPU_IMXRT
	_imxrt_platformInit();
#endif
}
