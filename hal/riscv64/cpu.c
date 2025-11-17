/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines (RISCV64)
 *
 * Copyright 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "include/errno.h"
#include "hal/hal.h"
#include "riscv64.h"
#include "dtb.h"

#include <arch/timer.h>


extern addr_t hal_relOffs;


struct hal_perHartData {
	unsigned long hartId;
	ptr_t kstack;
	ptr_t scratch;
	/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Definition in assembly" */
} __attribute__((packed, aligned(8))) hal_riscvHartData[MAX_CPU_COUNT];


static struct {
	volatile u32 cpuCnt;
	u32 cpusStarted;
} cpu_common;


/* bit operations */

/* TODO: use clz/ctz instructions */
unsigned int hal_cpuGetLastBit(unsigned long v)
{
	unsigned int lb = 63;

	if ((v & 0xffffffff00000000UL) == 0UL) {
		lb -= 32U;
		v = (((u64)v) << 32);
	}

	if ((v & 0xffff000000000000UL) == 0UL) {
		lb -= 16U;
		v = (v << 16);
	}

	if ((v & 0xff00000000000000UL) == 0UL) {
		lb -= 8U;
		v = (v << 8);
	}

	if ((v & 0xf000000000000000UL) == 0UL) {
		lb -= 4U;
		v = (v << 4);
	}

	if ((v & 0xc000000000000000UL) == 0UL) {
		lb -= 2U;
		v = (v << 2);
	}

	if ((v & 0x8000000000000000UL) == 0UL) {
		lb -= 1U;
	}

	return lb;
}


unsigned int hal_cpuGetFirstBit(unsigned long v)
{
	unsigned int fb = 0;

	if ((v & 0xffffffffUL) == 0UL) {
		fb += 32U;
		v = (v >> 32);
	}

	if ((v & 0xffffUL) == 0UL) {
		fb += 16U;
		v = (v >> 16);
	}

	if ((v & 0xffUL) == 0UL) {
		fb += 8U;
		v = (v >> 8);
	}

	if ((v & 0xfUL) == 0UL) {
		fb += 4U;
		v = (v >> 4);
	}

	if ((v & 0x3UL) == 0UL) {
		fb += 2U;
		v = (v >> 2);
	}

	if ((v & 0x1UL) == 0UL) {
		fb += 1U;
	}

	return fb;
}


/* context management */


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
int hal_cpuCreateContext(cpu_context_t **nctx, startFn_t start, void *kstack, size_t kstacksz, void *ustack, void *arg, hal_tls_t *tls)
{
	cpu_context_t *ctx;

	*nctx = NULL;
	if (kstack == NULL) {
		return -EINVAL;
	}

	if (kstacksz < sizeof(cpu_context_t)) {
		return -EINVAL;
	}

	/* Align user stack to 16 bytes */
	ustack = (void *)((ptr_t)ustack & ~0xfU);

	ctx = (cpu_context_t *)((char *)kstack + kstacksz - sizeof(cpu_context_t));

	/* clang-format off */
	__asm__ volatile("sd gp, %0" : "=m"(ctx->gp));
	/* clang-format on */

	ctx->ra = (u64)0;
	ctx->sp = (u64)kstack + kstacksz;

	ctx->t0 = 0;
	ctx->t1 = 0x0101010101010101U;
	ctx->t2 = 0x0202020202020202U;

	ctx->s0 = (u64)ctx;
	ctx->s1 = 0x0404040404040404U;
	ctx->a0 = (u64)arg;
	ctx->a1 = 0x0606060606060606U;

	ctx->a2 = 0x0707070707070707U;
	ctx->a3 = 0x0808080808080808U;
	ctx->a4 = 0x0909090909090909U;
	ctx->a5 = 0x0a0a0a0a0a0a0a0aU;

	ctx->a6 = 0x0b0b0b0b0b0b0b0bU;
	ctx->a7 = 0x0c0c0c0c0c0c0c0cU;
	ctx->s2 = 0x0d0d0d0d0d0d0d0dU;
	ctx->s3 = 0x0e0e0e0e0e0e0e0eU;

	ctx->s4 = 0x0f0f0f0f0f0f0f0fU;
	ctx->s5 = 0x1010101010101010U;
	ctx->s6 = 0x1111111111111111U;
	ctx->s7 = 0x1212121212121212U;

	ctx->s8 = 0x1313131313131313U;
	ctx->s9 = 0x1414141414141414U;
	ctx->s10 = 0x1515151515151515U;
	ctx->s11 = 0x1616161616161616U;

	ctx->t3 = 0x1717171717171717U;
	ctx->t4 = 0x1818181818181818U;
	ctx->t5 = 0x1919191919191919U;
	ctx->t6 = 0x1a1a1a1a1a1a1a1aU;

	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "Need to assign function address to processor register" */
	ctx->sepc = (u64)start;
	ctx->ksp = (u64)ctx;

	if (ustack != NULL) {
		ctx->sp = (u64)ustack;
		ctx->sstatus = (csr_read(sstatus) | SSTATUS_SPIE | SSTATUS_SUM) & ~(SSTATUS_SPP | SSTATUS_FS);
		ctx->tp = tls->tls_base;
	}
	else {
		ctx->sstatus = (csr_read(sstatus) | SSTATUS_SPIE | SSTATUS_SPP) & ~SSTATUS_FS;
		ctx->tp = 0;
	}

	*nctx = ctx;

	return EOK;
}


int hal_cpuPushSignal(void *kstack, void (*handler)(void), cpu_context_t *signalCtx, int n, unsigned int oldmask, const int src)
{
	cpu_context_t *ctx = (void *)((char *)kstack - sizeof(cpu_context_t));
	const struct stackArg args[] = {
		{ &ctx->sp, sizeof(ctx->sp) },
		{ &ctx->sepc, sizeof(ctx->sepc) },
		{ &signalCtx, sizeof(signalCtx) },
		{ &oldmask, sizeof(oldmask) },
		{ &n, sizeof(n) },
	};

	(void)src;

	hal_memcpy(signalCtx, ctx, sizeof(cpu_context_t));

	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "Need to assign function address to processor register" */
	signalCtx->sepc = (u64)handler;
	signalCtx->sp -= sizeof(cpu_context_t);

	hal_stackPutArgs((void **)&signalCtx->sp, sizeof(args) / sizeof(args[0]), args);

	return 0;
}


void hal_cpuSigreturn(void *kstack, void *ustack, cpu_context_t **ctx)
{
	(void)kstack;
	GETFROMSTACK(ustack, u64, (*ctx)->sepc, 2);
	GETFROMSTACK(ustack, u64, (*ctx)->sp, 3);
}


void _hal_cpuSetKernelStack(void *kstack)
{
	struct hal_perHartData *data = (void *)csr_read(sscratch);
	data->kstack = (ptr_t)kstack;
}


char *hal_cpuInfo(char *info)
{
	size_t i = 0, l;
	char *model, *compatible;

	dtb_getSystem(&model, &compatible);

	l = hal_strlen(model);
	hal_memcpy(info, model, l);
	i += l;

	(void)hal_strcpy(&info[i], " (");
	i += 2U;

	l = hal_strlen(compatible);
	hal_memcpy(&info[i], compatible, l);
	i += l;

	info[i++] = ')';
	i += hal_i2s(" - ", &info[i], hal_cpuGetCount(), 10U, 0U);
	(void)hal_strcpy(&info[i], " core");
	i += 5U;
	if (hal_cpuGetCount() > 1U) {
		(void)hal_strcpy(&info[i], "s");
		i += 1U;
	}
	info[i] = '\0';

	return info;
}


char *hal_cpuFeatures(char *features, size_t len)
{
	unsigned int n = 0;
	size_t i = 0, l;
	char *compatible, *isa, *mmu;
	u32 clock;

	while (dtb_getCPU(n++, &compatible, &clock, &isa, &mmu) == 0) {

		l = hal_strlen(compatible);
		hal_memcpy(features, compatible, l);
		i += l;

		i += hal_i2s("@", &features[i], (unsigned long)clock / 1000000UL, 10U, 0U);

		hal_memcpy(&features[i], "MHz", 3);
		i += 3U;

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

	features[i] = '\0';

	return features;
}


void hal_cpuLowPower(time_t us, spinlock_t *spinlock, spinlock_ctx_t *sc)
{
	hal_spinlockClear(spinlock, sc);
	hal_cpuHalt();
}


int hal_cpuLowPowerAvail(void)
{
	return 0;
}


void hal_cpuReboot(void)
{
	hal_sbiReset(SBI_RESET_TYPE_COLD, SBI_RESET_REASON_NONE);
}


/* cache management */


void hal_cleanDCache(ptr_t start, size_t len)
{
	(void)start;
	(void)len;
	/* TODO */
}


/* core management */


unsigned int hal_cpuGetCount(void)
{
	return cpu_common.cpuCnt;
}


unsigned int hal_cpuGetID(void)
{
	return (unsigned int)(((struct hal_perHartData *)csr_read(sscratch))->hartId);
}


void hal_cpuBroadcastIPI(unsigned int intr)
{
	(void)intr;

	unsigned long hart_mask = (1UL << cpu_common.cpuCnt) - 1U;
	hart_mask &= ~(1UL << hal_cpuGetID());

	(void)hal_sbiSendIPI(hart_mask, 0UL);
}


/* Sync instruction & data stores across SMP */
void hal_cpuSmpSync(void)
{
	unsigned long hart_mask;
	if (hal_cpuGetCount() > 1U) {
		hart_mask = (1UL << hal_cpuGetCount()) - 1U;
		RISCV_FENCE(rw, rw);
		hal_cpuInstrBarrier();
		hal_sbiRfenceI(hart_mask, 0UL);
	}
}


void hal_cpuRfenceI(void)
{
	unsigned long hart_mask;
	if (hal_cpuGetCount() > 1U) {
		hart_mask = (1UL << hal_cpuGetCount()) - 1U;
		hal_sbiRfenceI(hart_mask, 0UL);
	}
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
void hal_cpuLocalFlushTLB(u32 asid, const void *vaddr)
{
	(void)asid; /* TODO: ASID support */

	/* clang-format off */
	__asm__ volatile (
		"sfence.vma %0, zero"
		:
		: "r"(vaddr)
		: "memory"
	);
	/* clang-format on */
}


void hal_cpuRemoteFlushTLB(u32 asid, const void *vaddr, size_t size)
{
	size_t i;
	unsigned long hart_mask;

	if (hal_cpuGetCount() > 1U) {
		hart_mask = (1UL << hal_cpuGetCount()) - 1UL;
		(void)hal_sbiSfenceVma(hart_mask, 0UL, (unsigned long)vaddr, size);
	}
	else {
		for (i = 0; i < size; i += SIZE_PAGE) {
			hal_cpuLocalFlushTLB(asid, (void *)((unsigned long)vaddr + i));
		}
	}
}

/* parasoft-suppress-next-line MISRAC2012-RULE_8_4 "Definition in assembly" */
__attribute__((section(".init"))) void hal_cpuInitCore(void)
{
	hal_interruptsInitCore();
	hal_timerInitCore();
	(void)hal_cpuAtomicAdd(&cpu_common.cpusStarted, 1U);
}


__attribute__((section(".init"))) void _hal_cpuInit(void)
{
	long err;

	cpu_common.cpusStarted = 0;
	cpu_common.cpuCnt = 0;

	hal_cpuInitCore();

	/* Start other harts */
	do {
		err = hal_sbiHartStart(cpu_common.cpuCnt, pmap_getKernelStart(), hal_syspageAddr() - hal_relOffs).error;
		if ((err == SBI_SUCCESS) || (err == SBI_ERR_ALREADY_AVAILABLE)) {
			cpu_common.cpuCnt++;
		}
	} while (err != SBI_ERR_INVALID_PARAM);

	while (cpu_common.cpusStarted != cpu_common.cpuCnt) {
	}
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
void hal_cpuTlsSet(hal_tls_t *tls, cpu_context_t *ctx)
{
	(void)ctx;

	__asm__ volatile("mv tp, %0" ::"r"(tls->tls_base));
}
