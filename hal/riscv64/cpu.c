/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines (RISCV64)
 *
 * Copyright 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../../include/errno.h"
#include "cpu.h"
#include "spinlock.h"
#include "syspage.h"
#include "string.h"
#include "pmap.h"
#include "dtb.h"


extern int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


int hal_platformctl(void *ptr)
{
	return EOK;
}


int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg)
{
	cpu_context_t *ctx;

	*nctx = NULL;
	if (kstack == NULL)
		return -EINVAL;

	if (kstacksz < sizeof(cpu_context_t))
		return -EINVAL;

	ctx = (cpu_context_t *)(kstack + kstacksz - 2 * sizeof(cpu_context_t));

	__asm__ __volatile__ (
		"sd gp, %0"
		: "=m" (ctx->gp));

	ctx->pc = (u64)start;
	ctx->sp = (u64)ctx;

	ctx->t0 = 0;
	ctx->t1 = 0x0101010101010101;
	ctx->t2 = 0x0202020202020202;

	ctx->s0 = (u64)ctx;
	ctx->s1 = 0x0404040404040404;
	ctx->a0 = (u64)arg;
	ctx->a1 = 0x0606060606060606;

	ctx->a2 = 0x0707070707070707;
	ctx->a3 = 0x0808080808080808;
	ctx->a4 = 0x0909090909090909;
	ctx->a5 = 0x0a0a0a0a0a0a0a0a;

	ctx->a6 = 0x0b0b0b0b0b0b0b0b;
	ctx->a7 = 0x0c0c0c0c0c0c0c0c;
	ctx->s2 = 0x0d0d0d0d0d0d0d0d;
	ctx->s3 = 0x0e0e0e0e0e0e0e0e;

	ctx->s4 = 0x0f0f0f0f0f0f0f0f;
	ctx->s5 = 0x1010101010101010;
	ctx->s6 = 0x1111111111111111;
	ctx->s7 = 0x1212121212121212;

	ctx->s8 = 0x1313131313131313;
	ctx->s9 = 0x1414141414141414;
	ctx->s10 = 0x1515151515151515;
	ctx->s11 = 0x1616161616161616;

	ctx->t3 = 0x1717171717171717;
	ctx->t4 = 0x1818181818181818;
	ctx->t5 = 0x1919191919191919;
	ctx->t6 = 0x1a1a1a1a1a1a1a1a;

	ctx->sepc = (u64)start;
	ctx->ksp = (u64)ctx;

	if (ustack != NULL) {
		ctx->sp = (u64)ustack;
		ctx->sstatus = csr_read(sstatus) | SR_SPIE | SR_SUM;
		ctx->sscratch = (u64)ctx;
		ctx->tp = ctx->ksp;
	} else {
		ctx->sstatus = csr_read(sstatus) | SR_SPIE | SR_SPP;
		ctx->sscratch = 0;
		ctx->tp = 0;
	}

	*nctx = ctx;

	return EOK;
}


void _hal_cpuSetKernelStack(void *kstack)
{
	csr_write(sscratch, kstack);
}


void _hal_cpuInitCores(void)
{
}


char *hal_cpuInfo(char *info)
{
	unsigned int i = 0, l;
	char *model, *compatible;

	dtb_getSystem(&model, &compatible);

	l = hal_strlen(model);
	hal_memcpy(info, model, l);
	i += l;

	hal_memcpy(&info[i], " (", 2);
	i += 2;

	l = hal_strlen(compatible);
	hal_memcpy(&info[i], compatible, l);
	i += l;

	info[i++] = ')';
	info[i] = 0;

	return info;
}


char *hal_cpuFeatures(char *features, unsigned int len)
{
	unsigned int i = 0, l, n = 0;
	char *compatible, *isa, *mmu;
	u32 clock;


	while (!dtb_getCPU(n++, &compatible, &clock, &isa, &mmu)) {

		l = hal_strlen(compatible);
		hal_memcpy(features, compatible, l);
		i += l;

		i += hal_i2s("@", &features[i], clock / 1000000, 10, 0);

		hal_memcpy(&features[i], "MHz", 3);
		i += 3;

		features[i++] = '(';

		l = hal_strlen(isa);
		hal_memcpy(&features[i], isa, l);
		i += l;

		features[i++] = '+';

		l = hal_strlen(mmu);
		hal_memcpy(&features[i], mmu, l);
		i += l;

		features[i++] = ')';
		features[i++] = ' ';
	}

	features[i] = 0;

	return features;
}


void _hal_cpuInit(void)
{
	_hal_cpuInitCores();
}
