/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines
 *
 * Copyright 2012, 2017, 2020 Phoenix Systems
 * Copyright 2001, 2006 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Jan Sikorski, Kamil Amanowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../../include/errno.h"
#include "../../include/arch/ia32.h"
#include "cpu.h"
#include "spinlock.h"
#include "syspage.h"
#include "string.h"
#include "pmap.h"
#include "pci.h"


extern int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


struct {
	tss_t tss[256];
	char stacks[256][512];
	u32 dr5;
	volatile unsigned int ncpus;
} cpu;


/* context management */


u32 cpu_getEFLAGS(void)
{
	u32 eflags;

	__asm__ volatile(" \
		pushf; \
		popl %0"
					 : "=a"(eflags));

	return eflags;
}


int hal_cpuDebugGuard(u32 enable, u32 slot)
{
	/* guard 4 bytes read/write */
	u32 mask = (3 << (2 * slot)) | (0xf << (2 * slot + 16));

	/* exact breakpoint match */
	mask |= 3 << 8;

	if (slot > 3)
		return -EINVAL;

	if (enable)
		cpu.dr5 |= mask;
	else
		cpu.dr5 &= ~mask;

	__asm__ volatile("movl %0, %%dr5"
					 :
					 : "r"(cpu.dr5));

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

	/* Prepare initial kernel stack */
	ctx = (cpu_context_t *)(kstack + kstacksz - sizeof(cpu_context_t));
	hal_cpuRestore(ctx, ctx);

#ifndef NDEBUG
	ctx->dr0 = (u32)kstack + 16; /* protect bottom bytes of kstack */
	ctx->dr1 = 0;
	ctx->dr2 = 0;
	ctx->dr3 = 0;
#endif

	ctx->edi = 0;
	ctx->esi = 0;
	ctx->ebp = 0;
	ctx->edx = 0;
	ctx->ecx = 0;
	ctx->ebx = 0;
	ctx->eax = 0;
	ctx->gs = ustack ? SEL_UDATA : SEL_KDATA;
	ctx->fs = ustack ? SEL_UDATA : SEL_KDATA;
	ctx->es = ustack ? SEL_UDATA : SEL_KDATA;
	ctx->ds = ustack ? SEL_UDATA : SEL_KDATA;
	ctx->eip = (u32)start;
	ctx->cs = ustack ? SEL_UCODE : SEL_KCODE;

	/* Copy flags from current process and enable interrupts */
	ctx->eflags = (cpu_getEFLAGS() | 0x00000200 | 0x00003000); /* IOPL = 3 */

	/* Prepare user stack for user-level thread */
	if (ustack != NULL) {
		ctx->esp = (u32)ustack - 8;
		((u32 *)ctx->esp)[1] = (u32)arg;
		ctx->ss = SEL_UDATA;
	}

	/* Prepare kernel stack for kernel-level thread */
	else {
		ctx->ss = (u32)arg;
	}

	*nctx = ctx;

	return EOK;
}


int hal_cpuReschedule(spinlock_t *spinlock, spinlock_ctx_t *scp)
{
	int err;

	if (spinlock != NULL) {
		hal_cpuGetCycles((void *)&spinlock->e);

		/* Calculate maximum and minimum lock time */
		if ((cycles_t)(spinlock->e - spinlock->b) > spinlock->dmax)
			spinlock->dmax = spinlock->e - spinlock->b;

		if (spinlock->e - spinlock->b < spinlock->dmin)
			spinlock->dmin = spinlock->e - spinlock->b;
	}

	__asm__ volatile(
		"movl %1, %%eax;"
		"cmp $0, %%eax;"
		"je 1f;"

		"movl %3, %%eax;"
		"pushl %%eax;"
		"xorl %%eax, %%eax;"
		"incl %%eax;"
		"xchgl %2, %%eax;"
		"jmp 2f;"

		"1:;"
		"pushf;"

		"2:;"
		"pushl %%cs;"
		"cli;"
		"leal 3f, %%eax;"
		"pushl %%eax;"
		"movl $0, %%eax;"
		"call interrupts_pushContext;"

		"call _interrupts_multilockSet;"

		"leal 0(%%esp), %%eax;"
		"pushl $0;"
		"pushl %%eax;"
		"pushl $0;"
		"movl %4, %%eax;"
		"call *%%eax;"
		"cli;"
		"addl $12, %%esp;"
		"popl %%esp;"

		"call _interrupts_multilockClear;"
		"pushl %%esp;"

		"jmp interrupts_popContext;"

		"3:;"
		"movl %%eax, %0"
		: "=g"(err)
		: "m"(spinlock), "m"(spinlock->lock), "r"(*scp), "g"(threads_schedule)
		: "eax", "edx", "esp", "cc", "memory");

	return err;
}


void _hal_cpuSetKernelStack(void *kstack)
{
	cpu.tss[hal_cpuGetID()].ss0 = SEL_KDATA;
	cpu.tss[hal_cpuGetID()].esp0 = (u32)kstack;
}


/* core management */


unsigned int hal_cpuGetCount(void)
{
	return cpu.ncpus;
}


void _cpu_gdtInsert(unsigned int idx, u32 base, u32 limit, u32 type)
{
	u32 descrl, descrh;
	u32 *gdt;

	/* Modify limit for 4KB granularity */
	if (type & DBITS_4KB)
		limit = (limit >> 12);

	descrh = (base & 0xff000000) | (type & 0x00c00000) | (limit & 0x000f0000) |
		(type & 0x0000ff00) | ((base >> 16) & 0x000000ff);

	descrl = (base << 16) | (limit & 0xffff);

	gdt = (void *)*(u32 *)&syspage->gdtr[2];

	gdt[idx * 2] = descrl;
	gdt[idx * 2 + 1] = descrh;

	return;
}


void *_cpu_initCore(void)
{
	cpu.ncpus++;

	u32 a = *(u32 *)0xfee000f0;
	*(u32 *)0xfee000f0 = (a | 0x100);

	hal_memset(&cpu.tss[hal_cpuGetID()], 0, sizeof(tss_t));

	_cpu_gdtInsert(4 + cpu.ncpus, (u32)&cpu.tss[hal_cpuGetID()], sizeof(tss_t), DESCR_TSS);

	cpu.tss[hal_cpuGetID()].ss0 = SEL_KDATA;
	cpu.tss[hal_cpuGetID()].esp0 = (u32)&cpu.stacks[hal_cpuGetID()][511];

	/* Set task register */
	__asm__ volatile("ltr %%ax"
					 :
					 : "a"((4 + cpu.ncpus) * 8));

	return (void *)cpu.tss[hal_cpuGetID()].esp0;
}


void _hal_cpuInitCores(void)
{
	unsigned int i, k;

	/* Prepare descriptors for user segments */
	_cpu_gdtInsert(3, 0x00000000, VADDR_KERNEL, DESCR_UCODE);
	_cpu_gdtInsert(4, 0x00000000, VADDR_KERNEL, DESCR_UDATA);

	/* Initialize BSP */
	cpu.ncpus = 0;
	_cpu_initCore();

	*(u32 *)((void *)syspage->stack + VADDR_KERNEL - 4) = 0;

	for (;;) {
		k = cpu.ncpus;
		i = 0;
		while ((cpu.ncpus == k) && (++i < 50000000))
			;
		if (i >= 50000000)
			break;
	}
}


char *hal_cpuInfo(char *info)
{
	u32 nb, nx, v[4], a, fam, model;
	unsigned int i = 12;

	/* Get number of extended cpuid levels */
	hal_cpuid(0x80000000, 0, &nx, v + 1, v + 2, v + 3);
	nx &= 0x7fffffff;

	/* Get vendor and model */
	hal_cpuid(0, 0, &nb, (u32 *)&info[0], (u32 *)&info[8], (u32 *)&info[4]);
	info[i] = 0;

	hal_cpuid(1, 0, &a, v + 1, v + 2, v + 3);
	fam = (a >> 8) & 0xf;
	if (fam == 0xf)
		fam += (a >> 20) & 0xff;

	model = (a >> 4) & 0xf;
	if (fam == 6 || fam == 15)
		model |= (a >> 12) & 0xf0;

	i += hal_i2s(" Family ", &info[i], fam, 16, 0);
	i += hal_i2s(" Model ", &info[i], model, 16, 0);
	i += hal_i2s(" Stepping ", &info[i], a & 0xf, 16, 0);

	i += hal_i2s(" (", &info[i], nb, 10, 0);
	i += hal_i2s("/", &info[i], nx, 10, 0);
	info[i++] = ')';

	i += hal_i2s(", cores=", &info[i], cpu.ncpus, 10, 0);

	info[i] = 0;

	return info;
}


struct cpu_feature_t {
	const char *name;
	u32 eax;
	u8 reg;
	u8 offset; /* eax, ebx, ecx, edx */
};


static const struct cpu_feature_t cpufeatures[] = {
	{ "fpu", 1, 3, 0 },      /* x87 FPU insns */
	{ "de", 1, 3, 2 },       /* debugging ext: CR4.DE, DR4 DR5 traps */
	{ "pse", 1, 3, 3 },      /* 4MiB pages */
	{ "tsc", 1, 3, 4 },      /* RDTSC insn */
	{ "msr", 1, 3, 5 },      /* RDMSR/WRMSR insns */
	{ "pae", 1, 3, 6 },      /* PAE */
	{ "apic", 1, 3, 6 },     /* APIC present */
	{ "cx8", 1, 2, 8 },      /* CMPXCHG8B insn */
	{ "sep", 1, 2, 11 },     /* SYSENTER/SYSEXIT insns */
	{ "mtrr", 1, 3, 12 },    /* MTRRs */
	{ "pge", 1, 3, 13 },     /* global pages */
	{ "cmov", 1, 3, 15 },    /* CMOV insn */
	{ "pat", 1, 3, 16 },     /* PAT */
	{ "pse36", 1, 3, 17 },   /* 4MiB pages can reach beyond 4GiB */
	{ "psn", 1, 3, 18 },     /* CPU serial number enabled */
	{ "clflush", 1, 3, 19 }, /* CLFLUSH insn */
	{ "cx16", 1, 2, 13 },    /* CMPXCHG16B insn */
	{ "dca", 1, 2, 18 },     /* prefetch from MMIO */
	{ "xsave", 1, 2, 26 },   /* XSAVE/XRSTOR insns */
	{ "smep", 7, 1, 7 },     /* SMEP */
	{ "smap", 7, 1, 20 },    /* SMAP */
	{ "nx", -1, 3, 20 },     /* page execute disable bit */
	{
		NULL,
	}
};


char *hal_cpuFeatures(char *features, unsigned int len)
{
	u32 nb, nx, v[4], a;
	unsigned int i = 0, overflow = 0;
	const struct cpu_feature_t *p;
	unsigned int ln;

	/* Get number of basic cpuid levels */
	hal_cpuid(0, 0, &nb, v + 1, v + 2, v + 3);

	/* Get number of extended cpuid levels */
	hal_cpuid(0x80000000, 0, &nx, v + 1, v + 2, v + 3);
	nx &= 0x7fffffff;

	for (p = cpufeatures; p->name != NULL; ++p) {
		if (p->eax < 0 ? p->eax < -nx : p->eax > nb)
			continue;

		a = p->eax < 0 ? 0x80000000 - p->eax : p->eax;
		hal_cpuid(a, 0, v + 0, v + 1, v + 2, v + 3);

		if (v[p->reg] & (1 << p->offset)) {
			ln = hal_strlen(p->name);
			if (!overflow && (i + ln + 1 + 1 < len)) {
				features[i++] = '+';
				hal_memcpy(&features[i], p->name, ln);
				i += ln;
			}
			else if (!overflow) {
				overflow = 1;
				features[i++] = '|';
			}
		}
	}
	features[i] = 0;

	return features;
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = (platformctl_t *)ptr;

	switch (data->type) {
		case pctl_pci:
			if (data->action == pctl_get)
				return hal_pciGetDevice(&data->pci.id, &data->pci.dev, data->pci.caps);
			break;

		case pctl_busmaster:
			if (data->action == pctl_set)
				return hal_pciSetBusmaster(&data->busmaster.dev, data->busmaster.enable);
			break;
	}

	return -EINVAL;
}


void _hal_cpuInit(void)
{
	cpu.ncpus = 0;

	_hal_cpuInitCores();
#ifndef NDEBUG
	hal_cpuDebugGuard(1, 0);
//	hal_cpuDebugGuard(1, 1);
//	hal_cpuDebugGuard(1, 2);
//	hal_cpuDebugGuard(1, 3);
#endif
}
