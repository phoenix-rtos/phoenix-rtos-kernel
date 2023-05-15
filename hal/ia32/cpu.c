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
#include "../cpu.h"
#include "../spinlock.h"
#include "../string.h"
#include "../pmap.h"
#include "pci.h"
#include "ia32.h"
#include "halsyspage.h"
#include "../hal.h"


struct cpu_feature_t {
	const char *name;
	s32 eax;
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

	__asm__ volatile(
		"pushf; "
		"popl %0; "
	: "=a" (eflags));

	return eflags;
}


int hal_cpuCreateContext(cpu_context_t **nctx, void *start, void *kstack, size_t kstacksz, void *ustack, void *arg, hal_tls_t *tls)
{
	cpu_context_t *ctx;

	(void)tls;

	*nctx = NULL;
	if (kstack == NULL) {
		return -EINVAL;
	}

	if (kstacksz < sizeof(cpu_context_t)) {
		return -EINVAL;
	}

	/* Prepare initial kernel stack */
	ctx = (cpu_context_t *)(kstack + kstacksz - sizeof(cpu_context_t));
	hal_cpuRestore(ctx, ctx);

	hal_memset(&ctx->fpuContext, 0, sizeof(ctx->fpuContext));
	ctx->cr0Bits = CR0_TS_BIT; /* The process starts with unused FPU */
	ctx->edi = 0;
	ctx->esi = 0;
	ctx->ebp = 0;
	ctx->edx = 0;
	ctx->ecx = 0;
	ctx->ebx = 0;
	ctx->eax = 0;
	ctx->gs = SEL_TLS;
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


void _hal_cpuSetKernelStack(void *kstack)
{
	cpu.tss[hal_cpuGetID()].ss0 = SEL_KDATA;
	cpu.tss[hal_cpuGetID()].esp0 = (u32)kstack;
}


int hal_cpuPushSignal(void *kstack, void (*handler)(void), int n)
{
	cpu_context_t *ctx = (void *)((char *)kstack - sizeof(cpu_context_t));
	char *ustack = (char *)ctx->esp;

	PUTONSTACK(ustack, u32, ctx->eip);
	PUTONSTACK(ustack, int, n);

	ctx->eip = (u32)handler;
	ctx->esp = (u32)ustack;

	return 0;
}


void hal_longjmp(cpu_context_t *ctx)
{
	/* clang-format off */
	__asm__ volatile(" \
		cli; \
		movl %0, %%eax;\
		addl $4, %%eax;\
		movl %%eax, %%esp;\
		popl %%edi;\
		popl %%esi;\
		popl %%ebp;\
		popl %%edx;\
		popl %%ecx;\
		popl %%ebx;\
		popl %%eax;\
		popw %%gs;\
		popw %%fs;\
		popw %%es;\
		popw %%ds;\
		testl %[cr0ts], %c[fpuContextSize](%%esp);\
		movl %%eax, %c[fpuContextSize](%%esp);\
		movl %%cr0, %%eax;\
		jz .hal_longjmp_fpu;\
		orl %[cr0ts], %%eax;\
		mov %%eax, %%cr0;\
		addl %[fpuContextSize], %%esp;\
		popl %%eax;\
		iret;\
		.hal_longjmp_fpu: \
		andl %[not_cr0ts], %%eax;\
		mov %%eax, %%cr0;\
		frstor (%%esp);\
		addl %[fpuContextSize], %%esp;\
		popl %%eax;\
		iret"
	:
	: "g" (ctx), [cr0ts] "i" (CR0_TS_BIT), [not_cr0ts] "i" (~CR0_TS_BIT), [fpuContextSize] "i" (FPU_CONTEXT_SIZE));
	/* clang-format on */
}


void hal_jmp(void *f, void *kstack, void *stack, int argc)
{
	if (stack == NULL) {
		__asm__ volatile
		(" \
			movl %0, %%esp;\
			movl %1, %%eax;\
			call *%%eax"
		:
		:"r" (kstack), "r" (f));
	}
	else {
		__asm__ volatile
		(" \
			sti; \
			movl %1, %%eax;\
			movl %2, %%ebx;\
			movl %3, %%ecx;\
			movl %4, %%edx;\
			movl %0, %%esp;\
			pushl %%edx;\
			pushl %%ebx;\
			pushfl;\
			pushl %%ecx;\
			movw %%dx, %%ds;\
			movw %%dx, %%es;\
			movw %%dx, %%fs;\
			pushl %%eax;\
			iret"
		:
		:"g" (kstack), "g" (f), "g" (stack), "g" (SEL_UCODE), "g" (SEL_UDATA)
		:"eax", "ebx", "ecx", "edx");
	}
}


/* core management */


static void hal_cpuid(u32 leaf, u32 index, u32 *ra, u32 *rb, u32 *rc, u32 *rd)
{
	__asm__ volatile
	(" \
		cpuid"
	: "=a" (*ra), "=b" (*rb), "=c" (*rc), "=d" (*rd)
	: "a" (leaf), "c" (index)
	: "memory");
}


unsigned int hal_cpuGetCount(void)
{
	return cpu.ncpus;
}


static inline unsigned int _hal_cpuGetID(void)
{
	u32 id;

	/* 0xfee00020 - Local APIC ID Register */
	__asm__ volatile
	(" \
		movl (0xfee00020), %0"
	: "=r" (id));

	return id;
}


unsigned int hal_cpuGetID(void)
{
	u32 id = _hal_cpuGetID();

	return (id == 0xffffffff) ? 0 : (id >> 24);
}


void cpu_sendIPI(unsigned int cpu, unsigned int intr)
{
	if (_hal_cpuGetID() == 0xffffffff) {
		return;
	}

	/* 0xfee00300 - Interrupt Command Register (ICR); bits 0-31 */
	__asm__ volatile
	(" \
		movl %0, %%eax; \
		orl $0x000c4000, %%eax; \
		movl %%eax, (0xfee00300); \
	b0:; \
		btl $12, (0xfee00300); \
		jc b0"
	:
	:  "r" (intr));
}


static void _cpu_gdtInsert(unsigned int idx, u32 base, u32 limit, u32 type)
{
	u32 descrl, descrh;
	u32 *gdt;

	/* Modify limit for 4KB granularity */
	if ((type & DBITS_4KB) != 0) {
		limit = (limit >> 12);
	}

	descrh = (base & 0xff000000) | (type & 0x00c00000) | (limit & 0x000f0000) |
		(type & 0x0000ff00) | ((base >> 16) & 0x000000ff);

	descrl = (base << 16) | (limit & 0xffff);

	gdt = (void *)syspage->hs.gdtr.addr;

	gdt[idx * 2] = descrl;
	gdt[idx * 2 + 1] = descrh;

	return;
}


void *_cpu_initCore(void)
{
	const unsigned int id = hal_cpuGetID();
	/* 0xfee000f0 - Local APIC, Spurious Interrupt Vector Register */
	volatile u32 *p = (void *)0xfee000f0;

	cpu.ncpus++;

	*p = (*p | 0x100);

	hal_memset(&cpu.tss[id], 0, sizeof(tss_t));

	_cpu_gdtInsert(TLS_DESC_IDX + (id + 1), (u32)&cpu.tss[id], sizeof(tss_t), DESCR_TSS);

	cpu.tss[id].ss0 = SEL_KDATA;
	cpu.tss[id].esp0 = (u32)&cpu.stacks[id][511];

	/* Init FPU - set flags:
	   MP - FWAIT instruction does not ignore TS flag
	   TS - The first use of FPU generates device-not-available exception (#NM)
	   NE - FPU exceptions are handled internally*/
	/* clang-format off */
	__asm__ volatile (
		"fninit;"
		"movl %%cr0, %%eax;"
		"orl $0x2A, %%eax;"
		"mov %%eax, %%cr0;"
	:
	:
	: "eax");
	/* clang-format on */

	/* Set task register */
	__asm__ volatile(
		"ltr %0; "
	:: "r" ((u16)((TLS_DESC_IDX + (id + 1)) * 8)));

	return (void *)cpu.tss[id].esp0;
}


static void _hal_cpuInitCores(void)
{
	unsigned int i, k;

	/* Prepare descriptors for user segments */
	_cpu_gdtInsert(3, 0x00000000, VADDR_KERNEL, DESCR_UCODE);
	_cpu_gdtInsert(4, 0x00000000, VADDR_KERNEL, DESCR_UDATA);
	_cpu_gdtInsert(TLS_DESC_IDX, 0x00000000, VADDR_KERNEL, DESCR_TLS);

	/* Initialize BSP */
	cpu.ncpus = 0;
	_cpu_initCore();

	*(u32 *)(syspage->hs.stack + VADDR_KERNEL - 4) = 0;

	for (;;) {
		k = cpu.ncpus;
		i = 0;
		while ((cpu.ncpus == k) && (++i < 50000000))
			;
		if (i >= 50000000) {
			break;
		}
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
	if (fam == 0xf) {
		fam += (a >> 20) & 0xff;
	}

	model = (a >> 4) & 0xf;
	if ((fam == 6) || (fam == 15)) {
		model |= (a >> 12) & 0xf0;
	}

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


char *hal_cpuFeatures(char *features, unsigned int len)
{
	u32 nb, nx, v[4], a;
	unsigned int i = 0;
	const struct cpu_feature_t *p;
	unsigned int ln;

	/* Get number of basic cpuid levels */
	hal_cpuid(0, 0, &nb, v + 1, v + 2, v + 3);

	/* Get number of extended cpuid levels */
	hal_cpuid(0x80000000, 0, &nx, v + 1, v + 2, v + 3);
	nx &= 0x7fffffff;

	for (p = cpufeatures; p->name != NULL; ++p) {
		if ((p->eax < 0) ? (p->eax < -(s32)nx) : ((u32)p->eax > nb)) {
			continue;
		}

		a = (p->eax < 0) ? (0x80000000 - p->eax) : (u32)p->eax;
		hal_cpuid(a, 0, v + 0, v + 1, v + 2, v + 3);

		if (v[p->reg] & (1 << p->offset)) {
			ln = hal_strlen(p->name);
			if (i + ln + 1 + 1 < len) {
				if (i > 0) {
					features[i++] = '+';
				}
				hal_memcpy(&features[i], p->name, ln);
				i += ln;
			}
			else {
				if (i > 0) {
					i--;
				}
				features[i++] = '|';
				break;
			}
		}
	}
	features[i] = 0;

	return features;
}


void hal_cpuReboot(void)
{
	u8 status;
	u64 idtr0 = 0;
	u16 timeout;

	hal_cpuDisableInterrupts();

	/* 1. Try to reboot using keyboard controller (8042) */
	for (timeout = 0xffff; timeout != 0; --timeout) {
		status = hal_inb((void *)0x64);
		if ((status & 1) != 0) {
			(void)hal_inb((void *)0x60);
		}
		if ((status & 2) == 0) {
			break;
		}
	}
	hal_outb((void *)0x64, 0xfe);

	/* 2. Try to reboot by PCI reset */
	hal_outb((void *)0xcf9, 0xe);

	/* 3. Triple fault (interrupt with null idt) */
	__asm__ volatile(
		"lidt %0; "
		"int3; " ::"m"(idtr0));

	/* 4. Nothing worked, halt */
	for (;;) {
		hal_cpuHalt();
	}
}


int hal_platformctl(void *ptr)
{
	platformctl_t *data = (platformctl_t *)ptr;

	switch (data->type) {
		case pctl_pci:
			if (data->action == pctl_get) {
				return hal_pciGetDevice(&data->pci.id, &data->pci.dev, data->pci.caps);
			}
			break;

		case pctl_busmaster:
			if (data->action == pctl_set) {
				return hal_pciSetBusmaster(&data->busmaster.dev, data->busmaster.enable);
			}
			break;

		case pctl_reboot:
			if (data->action == pctl_set) {
				if (data->reboot.magic == PCTL_REBOOT_MAGIC) {
					hal_cpuReboot();
				}
			}
			break;

		default:
			break;
	}

	return -EINVAL;
}


void hal_cpuLowPower(time_t us, spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	hal_spinlockClear(spinlock, sc);
	hal_cpuHalt();
}


/* cache management */


void hal_cleanDCache(ptr_t start, size_t len)
{
	(void)start;
	(void)len;
	/* Shouldn't be needed on this arch */
}


void _hal_cpuInit(void)
{
	cpu.ncpus = 0;

	_hal_cpuInitCores();
}


void hal_cpuTlsSet(hal_tls_t *tls, cpu_context_t *ctx)
{
	ptr_t descrh, descrl;
	u32 type = DESCR_TLS;
	u32 *gdt;
	ptr_t base;
	ptr_t limit = VADDR_KERNEL;

	base = tls->tls_base + tls->tbss_sz + tls->tdata_sz;

	/* Update TLS entry in GDT with allocated page address */
	descrh = (base & 0xff000000) | (type & 0x00c00000) | (limit & 0x000f0000) |
		(type & 0x0000ff00) | ((base >> 16) & 0x000000ff);
	descrl = (base << 16) | (limit & 0xffff);

	gdt = (void *)syspage->hs.gdtr.addr;

	gdt[TLS_DESC_IDX * 2] = descrl;
	gdt[TLS_DESC_IDX * 2 + 1] = descrh;
}
