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

#include "include/errno.h"
#include "include/arch/ia32/ia32.h"
#include "hal/cpu.h"
#include "hal/spinlock.h"
#include "hal/string.h"
#include "hal/pmap.h"
#include "hal/hal.h"
#include "hal/tlb/tlb.h"
#include "pci.h"
#include "ia32.h"
#include "halsyspage.h"
#include "init.h"

#include <arch/tlb.h>


extern void hal_timerInitCore(const unsigned int id);


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


hal_cpu_t cpu;


/* context management */


static unsigned int hal_cpuGetTssIndex(void)
{
	return GDT_FREE_SEL_IDX + 2 * hal_cpuGetID();
}


unsigned int hal_cpuGetTlsIndex(void)
{
	return GDT_FREE_SEL_IDX + 2 * hal_cpuGetID() + 1;
}


/* hal_cpuSupervisorMode is called in asm code, so it must reside in .c file */

inline int hal_cpuSupervisorMode(cpu_context_t *ctx)
{
	return ((ctx->cs & 3) == 0);
}


u32 cpu_getEFLAGS(void)
{
	u32 eflags;

	/* clang-format off */
	__asm__ volatile (
		"pushf\n\t"
		"popl %0"
	: "=r" (eflags));
	/* clang-format on */

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
	ctx->gs = ustack ? 8 * hal_cpuGetTlsIndex() | 3 : SEL_KDATA;
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
	const unsigned int id = hal_cpuGetID();
	cpu.tss[id].ss0 = SEL_KDATA;
	cpu.tss[id].esp0 = (u32)kstack;
}


int hal_cpuPushSignal(void *kstack, void (*handler)(void), cpu_context_t *signalCtx, int n, const int src)
{
	cpu_context_t *ctx = (void *)((char *)kstack - sizeof(cpu_context_t));
	const struct stackArg args[] = {
		{ &ctx->esp, sizeof(ctx->esp) },
		{ &ctx->eip, sizeof(ctx->eip) },
		{ &signalCtx, sizeof(signalCtx) },
		{ &n, sizeof(n) },
	};

	(void)src;

	hal_memcpy(signalCtx, ctx, sizeof(cpu_context_t));

	signalCtx->eip = (u32)handler;
	signalCtx->esp -= sizeof(cpu_context_t);

	hal_stackPutArgs((void **)&signalCtx->esp, sizeof(args) / sizeof(args[0]), args);

	return 0;
}


void hal_cpuSigreturn(void *kstack, void *ustack, cpu_context_t **ctx)
{
	(void)kstack;
	GETFROMSTACK(ustack, u32, (*ctx)->eip, 2);
	GETFROMSTACK(ustack, u32, (*ctx)->esp, 3);
}


void hal_longjmp(cpu_context_t *ctx)
{
	hal_tlbFlushLocal(NULL);
	/* clang-format off */
	__asm__ volatile (
		"cli\n\t"
		"movl %0, %%eax\n\t"
		"addl $4, %%eax\n\t"
		"movl %%eax, %%esp\n\t"
		"movw 28(%%esp), %%dx\n\t"
		"cmpw %[kdata], %%dx\n\t"
		"je .Lignore_gs\n\t"
		"call hal_cpuGetTlsIndex\n\t"
		"shl $3, %%eax\n\t"
		"orb $3, %%al\n\t"
		"movw %%ax, 28(%%esp)\n\t"
		".Lignore_gs:"
		"popl %%edi\n\t"
		"popl %%esi\n\t"
		"popl %%ebp\n\t"
		"popl %%edx\n\t"
		"popl %%ecx\n\t"
		"popl %%ebx\n\t"
		"popl %%eax\n\t"
		"popw %%gs\n\t"
		"popw %%fs\n\t"
		"popw %%es\n\t"
		"popw %%ds\n\t"
		"testl %[cr0ts], %c[fpuContextSize](%%esp)\n\t"
		"movl %%eax, %c[fpuContextSize](%%esp)\n\t"
		"movl %%cr0, %%eax\n\t"
		"jz .Lhal_longjmp_fpu\n\t"
		"orl %[cr0ts], %%eax\n\t"
		"mov %%eax, %%cr0\n\t"
		"addl %[fpuContextSize], %%esp\n\t"
		"popl %%eax\n\t"
		"iret\n\n"
		".Lhal_longjmp_fpu:\n\t"
		"andl %[not_cr0ts], %%eax\n\t"
		"mov %%eax, %%cr0\n\t"
		"frstor (%%esp)\n\t"
		"addl %[fpuContextSize], %%esp\n\t"
		"popl %%eax\n\t"
		"iret\n"
	:
	: "g" (ctx), [cr0ts] "i" (CR0_TS_BIT), [not_cr0ts] "i" (~CR0_TS_BIT), [fpuContextSize] "i" (FPU_CONTEXT_SIZE), [kdata] "i" (SEL_KDATA));
	/* clang-format on */
}


void hal_jmp(void *f, void *kstack, void *ustack, size_t kargc, const arg_t *kargv)
{
	/* We support passing at most 4 args on every architecture. */
	struct stackArg args[4];
	size_t i;

	if (kargc > 4) {
		kargc = 4;
	}

	for (i = 0; i < kargc; i++) {
		/* Args on stack are in reverse order. */
		args[i].argp = &kargv[kargc - i - 1];
		args[i].sz = sizeof(arg_t);
	}
	hal_stackPutArgs(&kstack, kargc, args);

	hal_tlbFlushLocal(NULL);
	if (ustack == NULL) {
		/* clang-format off */
		__asm__ volatile (
			"movl %0, %%esp\n\t"
			"movl %1, %%eax\n\t"
			"call *%%eax"
		:
		: "r" (kstack), "r" (f));
		/* clang-format on */
	}
	else {
		/* clang-format off */
		__asm__ volatile (
			"sti\n\t"
			"movl %[func], %%eax\n\t"
			"movl %[ustack], %%ebx\n\t"
			"movl %[ucs], %%ecx\n\t"
			"movl %[uds], %%edx\n\t"
			"movl %[kstack], %%esp\n\t"
			"pushl %%edx\n\t"
			"pushl %%ebx\n\t"
			"pushfl\n\t"
			"pushl %%ecx\n\t"
			"movw %%dx, %%ds\n\t"
			"movw %%dx, %%es\n\t"
			"movw %%dx, %%fs\n\t"
			"shrl $16, %%edx\n\t"
			"movw %%dx, %%gs\n\t"
			"pushl %%eax\n\t"
			"iret"
		:
		: [kstack] "g" (kstack), [func] "g" (f), [ustack] "g" (ustack), [ucs] "g" (SEL_UCODE), [uds] "g" ((8 * hal_cpuGetTlsIndex() | 3) << 16 | SEL_UDATA)
		: "eax", "ebx", "ecx", "edx");
		/* clang-format on */
	}
}


/* core management */


unsigned int hal_cpuGetCount(void)
{
	return cpu.ncpus;
}


static inline unsigned int _hal_cpuGetID(void)
{
	if (hal_isLapicPresent() == 1) {
		return _hal_lapicRead(LAPIC_ID_REG) >> 24;
	}
	else {
		return 0;
	}
}


unsigned int hal_cpuGetID(void)
{
	u32 id = _hal_cpuGetID();
	unsigned int i;
	for (i = 0; i < cpu.ncpus; ++i) {
		if (cpu.cpus[i] == id) {
			return i;
		}
	}
	/* Critical error */
	return 0;
}

/* Sends IPI to everyone but self */
void hal_cpuBroadcastIPI(unsigned int intr)
{
	if (hal_isLapicPresent() == 1) {
		_hal_lapicWrite(LAPIC_ICR_REG_0_31, intr | 0xc4000);
		while ((_hal_lapicRead(LAPIC_ICR_REG_0_31) & (1 << 12)) != 0) {
		}
	}
}


void hal_cpuSendIPI(unsigned int cpu, unsigned int intrAndFlags)
{
	if (hal_isLapicPresent() == 1) {
		/* Set destination */
		/* TODO: Disable interrupts while we're writing */
		_hal_lapicWrite(LAPIC_ICR_REG_32_63, (cpu & 0xff) << 24);
		_hal_lapicWrite(LAPIC_ICR_REG_0_31, intrAndFlags & 0xcdfff);
		while ((_hal_lapicRead(LAPIC_ICR_REG_0_31) & (1 << 12)) != 0) {
		}
	}
}


static void _cpu_gdtInsert(unsigned int idx, u32 base, u32 limit, u32 type)
{
	u32 descrl, descrh;
	volatile u32 *gdt;

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
	hal_cpuAtomAdd(&cpu.readyCount, 1);

	if (hal_isLapicPresent() == 1) {
		_hal_lapicWrite(LAPIC_SPUR_IRQ_REG, _hal_lapicRead(LAPIC_SPUR_IRQ_REG) | 0x11ffu);
	}

	hal_memset(&cpu.tss[id], 0, sizeof(tss_t));

	_cpu_gdtInsert(hal_cpuGetTssIndex(), (u32)&cpu.tss[id], sizeof(tss_t), DESCR_TSS);
	_cpu_gdtInsert(hal_cpuGetTlsIndex(), 0x00000000, VADDR_KERNEL, DESCR_TLS);
	hal_cpuReloadTlsSegment();


	cpu.tss[id].ss0 = SEL_KDATA;
	cpu.tss[id].esp0 = (u32)&cpu.stacks[id][511];

	/* Init FPU - set flags:
	   MP - FWAIT instruction does not ignore TS flag
	   TS - The first use of FPU generates device-not-available exception (#NM)
	   NE - FPU exceptions are handled internally*/
	/* clang-format off */
	__asm__ volatile (
		"fninit\n\t"
		"movl %%cr0, %%eax\n\t"
		"orb $0x2a, %%al\n\t"
		"movl %%eax, %%cr0"
	:
	:
	: "eax");
	/* clang-format on */

	/* clang-format off */
	/* Set task register */
	__asm__ volatile (
		"ltr %0"
	:
	: "r" ((u16)(hal_cpuGetTssIndex() * 8)));
	/* clang-format on */

	hal_tlbInitCore(id);
	hal_timerInitCore(id);

	return (void *)cpu.tss[id].esp0;
}


static void _hal_cpuInitCores(void)
{
	/* Prepare descriptors for user segments */
	_cpu_gdtInsert(3, 0x00000000, VADDR_KERNEL, DESCR_UCODE);
	_cpu_gdtInsert(4, 0x00000000, VADDR_KERNEL, DESCR_UDATA);

	/* Initialize BSP */
	cpu.readyCount = 0;
	_hal_timerInit(SYSTICK_INTERVAL);
	_cpu_initCore();

	*(u32 *)(syspage->hs.stack + VADDR_KERNEL - 4) = 0;

	while (cpu.readyCount < cpu.ncpus) {
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
		status = hal_inb(PORT_PS2_COMMAND);
		if ((status & 1) != 0) {
			(void)hal_inb(PORT_PS2_DATA);
		}
		if ((status & 2) == 0) {
			break;
		}
	}
	hal_outb(PORT_PS2_COMMAND, 0xfe);

	/* 2. Try to reboot by PCI reset */
	hal_outb((void *)0xcf9, 0xe);

	/* 3. Triple fault (interrupt with null idt) */
	/* clang-format off */
	__asm__ volatile (
		"lidt %0\n\t"
		"int3"
	:
	: "m"(idtr0));
	/* clang-format on */

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
	_hal_cpuInitCores();
}


void hal_cpuTlsSet(hal_tls_t *tls, cpu_context_t *ctx)
{
	(void)ctx;
	hal_tlbFlushLocal(NULL);
	_cpu_gdtInsert(hal_cpuGetTlsIndex(), tls->tls_base + tls->tbss_sz + tls->tdata_sz, VADDR_KERNEL - tls->tls_base + tls->tbss_sz + tls->tdata_sz, DESCR_TLS);
	/* Reload the hidden gs register*/
	hal_cpuReloadTlsSegment();
}
