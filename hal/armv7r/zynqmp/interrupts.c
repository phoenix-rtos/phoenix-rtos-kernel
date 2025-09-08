/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Interrupt handling for ARM GIC v1
 *
 * Copyright 2021, 2024 Phoenix Systems
 * Author: Hubert Buczynski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/armv7r/armv7r.h"

#include "hal/cpu.h"
#include "hal/spinlock.h"
#include "hal/interrupts.h"
#include "hal/list.h"

#include "proc/userintr.h"


#define SPI_FIRST_IRQID 32

#define SGI_FLT_USE_LIST   0 /* Send SGI to CPUs according to targetList */
#define SGI_FLT_OTHER_CPUS 1 /* Send SGI to all CPUs except the one that called this function */
#define SGI_FLT_THIS_CPU   2 /* Send SGI to the CPU that called this function */

#define DEFAULT_PRIORITY 0x80


enum {
	/* Distributor registers */
	gicd_ctlr = 0x0,
	gicd_typer,
	gicd_iidr,
	gicd_igroupr0 = 0x20,     /* 6 registers */
	gicd_isenabler0 = 0x40,   /* 6 registers */
	gicd_icenabler0 = 0x60,   /* 6 registers */
	gicd_ispendr0 = 0x80,     /* 6 registers */
	gicd_icpendr0 = 0xa0,     /* 6 registers */
	gicd_isactiver0 = 0xc0,   /* 6 registers */
	gicd_icactiver0 = 0xe0,   /* 6 registers */
	gicd_ipriorityr0 = 0x100, /* 48 registers */
	gicd_itargetsr0 = 0x200,  /* 48 registers */
	gicd_icfgr0 = 0x300,      /* 12 registers */
	gicd_ppisr = 0x340,
	gicd_spisr0, /* 5 registers */
	gicd_sgir = 0x3c0,
	gicd_cpendsgir0 = 0x3c4, /* 4 registers */
	gicd_spendsgir0 = 0x3c8, /* 4 registers */
	gicd_pidr4 = 0x3f4,      /* 4 registers */
	gicd_pidr0 = 0x3f8,      /* 4 registers */
	gicd_cidr0 = 0x3fc,      /* 4 registers */
};


enum {
	/* CPU interface registers */
	gicc_ctlr = 0x0,
	gicc_pmr,
	gicc_bpr,
	gicc_iar,
	gicc_eoir,
	gicc_rpr,
	gicc_hppir,
	gicc_abpr,
	gicc_aiar,
	gicc_aeoir,
	gicc_ahppir,
	gicc_apr0 = 0x34,
	gicc_nsapr0 = 0x38,
	gicc_iidr = 0x3f,
};


struct {
	volatile u32 *gicd;
	volatile u32 *gicc;
	spinlock_t spinlock[SIZE_INTERRUPTS];
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts_common;


void _hal_interruptsInitPerCPU(void);

extern int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


int interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	int reschedule = 0;
	spinlock_ctx_t sc;

	u32 ciarValue = *(interrupts_common.gicc + gicc_iar);
	n = ciarValue & 0x3ff;

	if (n >= SIZE_INTERRUPTS) {
		return 0;
	}

	hal_spinlockSet(&interrupts_common.spinlock[n], &sc);

	interrupts_common.counters[n]++;

	if ((h = interrupts_common.handlers[n]) != NULL) {
		do {
			hal_cpuSetGot(h->got);
			reschedule |= h->f(n, ctx, h->data);
		} while ((h = h->next) != interrupts_common.handlers[n]);
	}

	if (reschedule) {
		threads_schedule(n, ctx, NULL);
	}

	*(interrupts_common.gicc + gicc_eoir) = ciarValue;

	hal_spinlockClear(&interrupts_common.spinlock[n], &sc);

	return reschedule;
}


static void interrupts_enableIRQ(unsigned int irqn)
{
	unsigned int irq_reg = irqn / 32;
	unsigned int irq_offs = irqn % 32;
	*(interrupts_common.gicd + gicd_isenabler0 + irq_reg) = 1u << irq_offs;
}


static void interrupts_disableIRQ(unsigned int irqn)
{
	unsigned int irq_reg = irqn / 32;
	unsigned int irq_offs = irqn % 32;
	*(interrupts_common.gicd + gicd_icenabler0 + irq_reg) = 1u << irq_offs;
}


static void interrupts_setConf(unsigned int irqn, u32 conf)
{
	unsigned int irq_reg = irqn / 16;
	unsigned int irq_offs = (irqn % 16) * 2;
	u32 mask;

	mask = *(interrupts_common.gicd + gicd_icfgr0 + irq_reg) & ~(0x3 << irq_offs);
	*(interrupts_common.gicd + gicd_icfgr0 + irq_reg) = mask | ((conf & 0x3) << irq_offs);
}


void interrupts_setCPU(unsigned int irqn, unsigned int cpuMask)
{
	unsigned int irq_reg = irqn / 4;
	unsigned int irq_offs = (irqn % 4) * 8;
	u32 mask;

	mask = *(interrupts_common.gicd + gicd_itargetsr0 + irq_reg) & ~(0xff << irq_offs);
	*(interrupts_common.gicd + gicd_itargetsr0 + irq_reg) = mask | ((cpuMask & 0xff) << irq_offs);
}


static void interrupts_setPriority(unsigned int irqn, u32 priority)
{
	unsigned int irq_reg = irqn / 4;
	unsigned int irq_offs = (irqn % 4) * 8;
	u32 mask = *(interrupts_common.gicd + gicd_ipriorityr0 + irq_reg) & ~(0xff << irq_offs);

	*(interrupts_common.gicd + gicd_ipriorityr0 + irq_reg) = mask | ((priority & 0xff) << irq_offs);
}


static inline u32 interrupts_getPriority(unsigned int irqn)
{
	unsigned int irq_reg = irqn / 4;
	unsigned int irq_offs = (irqn % 4) * 8;

	return (*(interrupts_common.gicd + gicd_ipriorityr0 + irq_reg) >> irq_offs) & 0xff;
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if ((h == NULL) || (h->f == NULL) || (h->n >= SIZE_INTERRUPTS)) {
		return -1;
	}

	hal_spinlockSet(&interrupts_common.spinlock[h->n], &sc);
	h->got = hal_cpuGetGot();
	HAL_LIST_ADD(&interrupts_common.handlers[h->n], h);

	interrupts_setPriority(h->n, DEFAULT_PRIORITY);
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

	if ((h == NULL) || (h->f == NULL) || (h->n >= SIZE_INTERRUPTS)) {
		return -1;
	}

	hal_spinlockSet(&interrupts_common.spinlock[h->n], &sc);
	HAL_LIST_REMOVE(&interrupts_common.handlers[h->n], h);

	if (interrupts_common.handlers[h->n] == NULL) {
		interrupts_disableIRQ(h->n);
	}

	hal_spinlockClear(&interrupts_common.spinlock[h->n], &sc);

	return 0;
}


int _interrupts_gicv2_classify(unsigned int irqn)
{
	/* ZynqMP specific: most interrupts are high level, some are reserved.
	 * PL to PS interrupts can be either high level or rising edge, here we configure
	 * lower half as high level, upper half as rising edge */
	if ((irqn < 40) || ((irqn >= 129) && (irqn <= 135))) {
		return 0;
	}
	else if ((irqn >= 136) && (irqn <= 143)) {
		return 3;
	}
	else {
		return 1;
	}
}


/* Function initializes interrupt handling */
void _hal_interruptsInit(void)
{
	u32 i;

	interrupts_common.gicd = (void *)0xf9000000;
	interrupts_common.gicc = (void *)0xf9001000;

	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		interrupts_common.handlers[i] = NULL;
		interrupts_common.counters[i] = 0;
		hal_spinlockCreate(&interrupts_common.spinlock[i], "interrupts");
	}

	/* Clear pending and disable interrupts */
	for (i = 0; i < (SIZE_INTERRUPTS + 31) / 32; i++) {
		*(interrupts_common.gicd + gicd_icenabler0 + i) = 0xffffffff;
		*(interrupts_common.gicd + gicd_icpendr0 + i) = 0xffffffff;
		*(interrupts_common.gicd + gicd_icactiver0 + i) = 0xffffffff;
	}

	for (i = 0; i < 4; i++) {
		*(interrupts_common.gicd + gicd_cpendsgir0 + i) = 0xffffffff;
	}

	/* Disable distributor */
	*(interrupts_common.gicd + gicd_ctlr) &= ~0x3;

	/* Set default priorities - 128 for the SGI (IRQID: 0 - 15), PPI (IRQID: 16 - 31), SPI (IRQID: 32 - 188) */
	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		interrupts_setPriority(i, DEFAULT_PRIORITY);
	}

	/* Set required configuration and CPU mask */
	for (i = SPI_FIRST_IRQID; i < SIZE_INTERRUPTS; ++i) {
		interrupts_setConf(i, _interrupts_gicv2_classify(i));
		interrupts_setCPU(i, 0x1);
	}

	/* enable_secure = 1 */
	*(interrupts_common.gicd + gicd_ctlr) |= 0x3;

	*(interrupts_common.gicc + gicc_ctlr) &= ~0x3;

	/* Initialize CPU Interface of the gic
	 * Set the maximum priority mask and binary point */
	*(interrupts_common.gicc + gicc_bpr) = 3;
	*(interrupts_common.gicc + gicc_pmr) = 0xff;

	/* EnableGrp0 = 1; EnableGrp1 = 1; AckCtl = 1; FIQEn = 1 in secure mode
	 * EnableGrp1 = 1 in non-secure mode, other bits are ignored */
	*(interrupts_common.gicc + gicc_ctlr) = *(interrupts_common.gicc + gicc_ctlr) | 0x7;
}


static void hal_cpuSendSGI(u8 targetFilter, u8 targetList, u8 intID)
{
	*(interrupts_common.gicd + gicd_sgir) = ((targetFilter & 0x3) << 24) | (targetList << 16) | (intID & 0xf);
	hal_cpuDataSyncBarrier();
}


void hal_cpuBroadcastIPI(unsigned int intr)
{
	hal_cpuSendSGI(SGI_FLT_OTHER_CPUS, 0, intr);
}
