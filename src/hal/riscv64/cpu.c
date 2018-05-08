/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines (RISCV64)
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../../../include/errno.h"
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
	*nctx = NULL;
	if (kstack == NULL)
		return -EINVAL;

	if (kstacksz < sizeof(cpu_context_t))
		return -EINVAL;

	return EOK;
}


int hal_cpuReschedule(spinlock_t *lock)
{
	return EOK;
}


void _hal_cpuSetKernelStack(void *kstack)
{
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
