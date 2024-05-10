/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception and interrupt handling for Zynq-7000
 *
 * Copyright 2021 Phoenix Systems
 * Author: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/armv7a/armv7a.h"

#include "hal/cpu.h"
#include "hal/spinlock.h"
#include "hal/interrupts.h"
#include "hal/list.h"

#include "proc/userintr.h"


#define SIZE_INTERRUPTS 95
#define SIZE_HANDLERS   4
#define SPI_FIRST_IRQID 32

#define SGI_FLT_USE_LIST   0 /* Send SGI to CPUs according to targetList */
#define SGI_FLT_OTHER_CPUS 1 /* Send SGI to all CPUs except the one that called this function */
#define SGI_FLT_THIS_CPU   2 /* Send SGI to the CPU that called this function */


/* clang-format off */
enum {
	/* Interrupt interface registers */
	cicr = 0x40, cpmr, cbpr, ciar, ceoir, crpr, chpir, cabpr,
	/* Distributor registers */
	ddcr = 0x400, dictr, diidr, disr0 = 0x420, /* 2 registers */	diser0 = 0x440, /* 2 registers */ dicer0 = 0x460, /* 2 registers */
	dispr0 = 0x480, /* 2 registers */ dicpr0 = 0x4a0, /* 2 registers */ dabr0 = 0x4c0, /* 2 registers */ dipr0 = 0x500, /* 24 registers */
	diptr0 = 0x600, /* 24 registers */ dicfr0 = 0x700, /* 6 registers */ ppi_st = 0x740, spi_st0, spi_st1, dsgir = 0x7c0, pidr4 = 0x7f4,
	pidr5, pidr6, pidr7, pidr0, pidr1, pidr2, pidr3, cidr0, cidr1, cidr2, cidr3
};


/* Type of interrupt's configuration */
enum {
	reserved = 0, high_lvl = 1, rising_edge = 3
};
/* clang-format on */


struct {
	volatile u32 *gic;
	spinlock_t spinlock[SIZE_INTERRUPTS];
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts_common;


/* Required configuration for SPI (Shared Peripheral Interrupts IRQID[32:95]) */
static const u8 spiConf[] = {
	/* IRQID: 32-39 */ rising_edge, rising_edge, high_lvl, high_lvl, reserved, high_lvl, high_lvl, high_lvl,
	/* IRQID: 40-47 */ high_lvl, rising_edge, high_lvl, high_lvl, high_lvl, high_lvl, high_lvl, high_lvl,
	/* IRQID: 48-55 */ high_lvl, high_lvl, high_lvl, high_lvl, high_lvl, high_lvl, high_lvl, rising_edge,
	/* IRQID: 56-63 */ high_lvl, high_lvl, high_lvl, high_lvl, high_lvl, rising_edge, rising_edge, rising_edge,
	/* IRQID: 64-71 */ rising_edge, rising_edge, rising_edge, rising_edge, rising_edge, high_lvl, high_lvl, high_lvl,
	/* IRQID: 72-79 */ high_lvl, high_lvl, high_lvl, high_lvl, high_lvl, high_lvl, rising_edge, high_lvl,
	/* IRQID: 80-87 */ high_lvl, high_lvl, high_lvl, high_lvl, high_lvl, high_lvl, high_lvl, high_lvl,
	/* IRQID: 88-95 */ high_lvl, high_lvl, high_lvl, high_lvl, rising_edge, reserved, reserved, reserved
};

void _hal_interruptsInitPerCPU(void);

extern int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);

extern unsigned int _end;


int interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0;
	spinlock_ctx_t sc;

	u32 ciarValue = *(interrupts_common.gic + ciar);
	n = ciarValue & 0x3ff;

	if (n >= SIZE_INTERRUPTS) {
		return 0;
	}

	hal_spinlockSet(&interrupts_common.spinlock[n], &sc);

	interrupts_common.counters[n]++;

	if ((h = interrupts_common.handlers[n]) != NULL) {
		do
			reschedule |= h->f(n, ctx, h->data);
		while ((h = h->next) != interrupts_common.handlers[n]);
	}

	if (reschedule)
		threads_schedule(n, ctx, NULL);

	*(interrupts_common.gic + ceoir) = ciarValue;

	hal_spinlockClear(&interrupts_common.spinlock[n], &sc);

	return reschedule;
}


static void interrupts_enableIRQ(unsigned int irqn)
{
	*(interrupts_common.gic + diser0 + (irqn >> 5)) = 1 << (irqn & 0x1f);
}


static void interrupts_disableIRQ(unsigned int irqn)
{
	*(interrupts_common.gic + dicer0 + (irqn >> 5)) = 1 << (irqn & 0x1f);
}


static void interrupts_setConf(unsigned int irqn, u32 conf)
{
	u32 mask;

	mask = *(interrupts_common.gic + dicfr0 + (irqn >> 4)) & ~(0x3 << ((irqn & 0xf) << 1));
	*(interrupts_common.gic + dicfr0 + (irqn >> 4)) = mask | ((conf & 0x3) << ((irqn & 0xf) << 1));
}


static void interrupts_setCPU(unsigned int irqn, u32 cpuID)
{
	u32 mask;

	mask = *(interrupts_common.gic + diptr0 + (irqn >> 2)) & ~(0x3 << ((irqn & 0x3) << 3));
	*(interrupts_common.gic + diptr0 + (irqn >> 2)) = mask | ((cpuID & 0x3) << ((irqn & 0x3) << 3));
}


static void interrupts_setPriority(unsigned int irqn, u32 priority)
{
	u32 mask = *(interrupts_common.gic + dipr0 + (irqn >> 2)) & ~(0xff << ((irqn & 0x3) << 3));

	*(interrupts_common.gic + dipr0 + (irqn >> 2)) = mask | ((priority & 0xff) << ((irqn & 0x3) << 3));
}


static inline u32 interrupts_getPriority(unsigned int irqn)
{
	return (*(interrupts_common.gic + dipr0 + (irqn >> 2)) >> ((irqn & 0x3) << 3)) & 0xff;
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS)
		return -1;

	hal_spinlockSet(&interrupts_common.spinlock[h->n], &sc);
	HAL_LIST_ADD(&interrupts_common.handlers[h->n], h);

	interrupts_setPriority(h->n, 0xa);
	interrupts_setCPU(h->n, 0x1);
	interrupts_enableIRQ(h->n);

	hal_spinlockClear(&interrupts_common.spinlock[h->n], &sc);

	return 0;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	hal_strncpy(features, "Using GIC interrupt controller", len);
	features[len - 1] = 0;

	return features;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if (h == NULL || h->f == NULL || h->n >= SIZE_INTERRUPTS)
		return -1;

	hal_spinlockSet(&interrupts_common.spinlock[h->n], &sc);
	HAL_LIST_REMOVE(&interrupts_common.handlers[h->n], h);

	if (interrupts_common.handlers[h->n] == NULL)
		interrupts_disableIRQ(h->n);

	hal_spinlockClear(&interrupts_common.spinlock[h->n], &sc);

	return 0;
}


/* Function initializes interrupt handling */
void _hal_interruptsInit(void)
{
	u32 i;

	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		interrupts_common.handlers[i] = NULL;
		interrupts_common.counters[i] = 0;
		hal_spinlockCreate(&interrupts_common.spinlock[i], "interrupts");
	}

	interrupts_common.gic = (void *)(((u32)&_end + (5 * SIZE_PAGE) - 1) & ~(SIZE_PAGE - 1));

	/* Initialize Distributor of the gic
	 * enable_secure = 0 */
	*(interrupts_common.gic + ddcr) &= ~0x3;

	/* Set default priorities - 10 for the SGI (IRQID: 0 - 15), PPI (IRQID: 16 - 31), SPI (IRQID: 32 - 95) */
	for (i = 0; i < SIZE_INTERRUPTS; ++i)
		interrupts_setPriority(i, 0xa);

	/* Set required configuration and CPU_0 as a default processor */
	for (i = SPI_FIRST_IRQID; i < SIZE_INTERRUPTS; ++i) {
		interrupts_setConf(i, spiConf[i - SPI_FIRST_IRQID]);
		interrupts_setCPU(i, 0x1);
	}

	/* SGI and PPI interrupts are fixed to always be on both CPUs */

	/* Disable interrupts */
	*(interrupts_common.gic + dicer0) = 0xffffffff;
	*(interrupts_common.gic + dicer0 + 1) = 0xffffffff;
	*(interrupts_common.gic + dicer0 + 2) = 0xffffffff;

	/* enable_secure = 1 */
	*(interrupts_common.gic + ddcr) |= 0x3;

	_hal_interruptsInitPerCPU();
}


void _hal_interruptsInitPerCPU(void)
{
	*(interrupts_common.gic + cicr) &= ~0x3;

	/* Initialize CPU Interface of the gic
	 * set the maximum priority mask */
	*(interrupts_common.gic + cpmr) |= 0x1f;

	/* EnableS = 1; EnableNS = 1; AckCtl = 1; FIQEn = 0 */
	*(interrupts_common.gic + cicr) |= 0x7;
}


static void hal_cpuSendSGI(u8 targetFilter, u8 targetList, u8 intID)
{
	*(interrupts_common.gic + dsgir) = ((targetFilter & 0x3) << 24) | (targetList << 16) | (intID & 0xf);
	hal_cpuDataMemoryBarrier();
}


void hal_cpuBroadcastIPI(unsigned int intr)
{
	hal_cpuSendSGI(SGI_FLT_OTHER_CPUS, 0, intr);
}
