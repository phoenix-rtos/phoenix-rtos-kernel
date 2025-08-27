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
	gicd_ctlr = 0x0U,
	gicd_typer,
	gicd_iidr,
	gicd_igroupr0 = 0x20U,     /* 6 registers */
	gicd_isenabler0 = 0x40U,   /* 6 registers */
	gicd_icenabler0 = 0x60U,   /* 6 registers */
	gicd_ispendr0 = 0x80U,     /* 6 registers */
	gicd_icpendr0 = 0xa0U,     /* 6 registers */
	gicd_isactiver0 = 0xc0U,   /* 6 registers */
	gicd_icactiver0 = 0xe0U,   /* 6 registers */
	gicd_ipriorityr0 = 0x100U, /* 48 registers */
	gicd_itargetsr0 = 0x200U,  /* 48 registers */
	gicd_icfgr0 = 0x300U,      /* 12 registers */
	gicd_ppisr = 0x340U,
	gicd_spisr0, /* 5 registers */
	gicd_sgir = 0x3c0U,
	gicd_cpendsgir0 = 0x3c4U, /* 4 registers */
	gicd_spendsgir0 = 0x3c8U, /* 4 registers */
	gicd_pidr4 = 0x3f4U,      /* 4 registers */
	gicd_pidr0 = 0x3f8U,      /* 4 registers */
	gicd_cidr0 = 0x3fcU,      /* 4 registers */
};


enum {
	/* CPU interface registers */
	gicc_ctlr = 0x0U,
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
	gicc_apr0 = 0x34U,
	gicc_nsapr0 = 0x38U,
	gicc_iidr = 0x3fU,
};


static struct {
	volatile u32 *gicd;
	volatile u32 *gicc;
	spinlock_t spinlock[SIZE_INTERRUPTS];
	intr_handler_t *handlers[SIZE_INTERRUPTS];
	unsigned int counters[SIZE_INTERRUPTS];
} interrupts_common;


void _hal_interruptsInitPerCPU(void);

extern int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


/* parasoft-begin-suppress MISRAC2012-RULE_8_4 "Function is used externaly within assembler code" */
int interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	intr_handler_t *h;
	unsigned int reschedule = 0;
	spinlock_ctx_t sc;

	u32 ciarValue = *(interrupts_common.gicc + gicc_iar);
	n = ciarValue & 0x3ffU;

	/* MISRA Rule 10.4: changed type by adding U in definition of SIZE_INTERRUPTS*/
	if (n >= SIZE_INTERRUPTS) {
		return 0;
	}

	hal_spinlockSet(&interrupts_common.spinlock[n], &sc);

	interrupts_common.counters[n]++;

	if ((h = interrupts_common.handlers[n]) != NULL) {
		do {
			reschedule |= (unsigned int)h->f(n, ctx, h->data);
		} while ((h = h->next) != interrupts_common.handlers[n]);
	}

	if (reschedule != 0U) {
		/* MISRA Rule 17.7: Unused returned value, added (void)*/
		(void)threads_schedule(n, ctx, NULL);
	}

	*(interrupts_common.gicc + gicc_eoir) = ciarValue;

	hal_spinlockClear(&interrupts_common.spinlock[n], &sc);

	return (int)reschedule;
}
/* parasoft-end-suppress MISRAC2012-RULE_8_4 */

static void interrupts_enableIRQ(unsigned int irqn)
{
	unsigned int irq_reg = irqn / 32U;
	unsigned int irq_offs = irqn % 32U;
	*(interrupts_common.gicd + gicd_isenabler0 + irq_reg) = 1UL << irq_offs;
}


static void interrupts_disableIRQ(unsigned int irqn)
{
	unsigned int irq_reg = irqn / 32U;
	unsigned int irq_offs = irqn % 32U;
	*(interrupts_common.gicd + gicd_icenabler0 + irq_reg) = 1UL << irq_offs;
}


static void interrupts_setConf(unsigned int irqn, u32 conf)
{
	unsigned int irq_reg = irqn / 16U;
	unsigned int irq_offs = (irqn % 16U) * 2U;
	u32 mask;

	mask = *(interrupts_common.gicd + gicd_icfgr0 + irq_reg) & ~(0x3U << irq_offs);
	*(interrupts_common.gicd + gicd_icfgr0 + irq_reg) = mask | ((conf & 0x3U) << irq_offs);
}


void interrupts_setCPU(unsigned int irqn, unsigned int cpuMask)
// TBD_Bart
{
	unsigned int irq_reg = irqn / 4U;
	unsigned int irq_offs = (irqn % 4U) * 8U;
	u32 mask;

	mask = *(interrupts_common.gicd + gicd_itargetsr0 + irq_reg) & ~(0xffU << irq_offs);
	*(interrupts_common.gicd + gicd_itargetsr0 + irq_reg) = mask | ((cpuMask & 0xffU) << irq_offs);
}


static void interrupts_setPriority(unsigned int irqn, u32 priority)
{
	unsigned int irq_reg = irqn / 4U;
	unsigned int irq_offs = (irqn % 4U) * 8U;
	u32 mask = *(interrupts_common.gicd + gicd_ipriorityr0 + irq_reg) & ~(0xffU << irq_offs);

	*(interrupts_common.gicd + gicd_ipriorityr0 + irq_reg) = mask | ((priority & 0xffU) << irq_offs);
}


static inline u32 interrupts_getPriority(unsigned int irqn)
{
	unsigned int irq_reg = irqn / 4U;
	unsigned int irq_offs = (irqn % 4U) * 8U;

	return (*(interrupts_common.gicd + gicd_ipriorityr0 + irq_reg) >> irq_offs) & 0xffU;
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if ((h == NULL) || (h->f == NULL) || (h->n >= SIZE_INTERRUPTS)) {
		return -1;
	}

	hal_spinlockSet(&interrupts_common.spinlock[h->n], &sc);
	HAL_LIST_ADD(&interrupts_common.handlers[h->n], h);

	interrupts_setPriority(h->n, DEFAULT_PRIORITY);
	interrupts_setCPU(h->n, 0x1);
	interrupts_enableIRQ(h->n);

	hal_spinlockClear(&interrupts_common.spinlock[h->n], &sc);

	return 0;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	/* MISRA Rule 17.7: Unused returned value, added (void)*/
	(void)hal_strncpy(features, "Using GIC interrupt controller", len);
	/* MISRA Rule 10.4: changed type by adding U; MISRA Rule 10.7: 0 changed to '\0'*/
	features[len - 1U] = '\0';

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
	if ((irqn < 40U) || ((irqn >= 129U) && (irqn <= 135U))) {
		return 0;
	}
	else if ((irqn >= 136U) && (irqn <= 143U)) {
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
	/* MISRA Rule 11.6: Casted 0xf9000000 and 0xf9001000 to a pointer type*/
	interrupts_common.gicd = (void *)(unsigned int *)0xf9000000U;
	interrupts_common.gicc = (void *)(unsigned int *)0xf9001000U;

	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		interrupts_common.handlers[i] = NULL;
		interrupts_common.counters[i] = 0;
		hal_spinlockCreate(&interrupts_common.spinlock[i], "interrupts");
	}

	/* Clear pending and disable interrupts */
	/* MISRA Rule 10.4: changed type by adding U */
	for (i = 0; i < (SIZE_INTERRUPTS + 31U) / 32U; i++) {
		*(interrupts_common.gicd + gicd_icenabler0 + i) = 0xffffffffU;
		*(interrupts_common.gicd + gicd_icpendr0 + i) = 0xffffffffU;
		*(interrupts_common.gicd + gicd_icactiver0 + i) = 0xffffffffU;
	}

	/* MISRA Rule 10.4: changed type by adding U */
	for (i = 0; i < 4U; i++) {
		*(interrupts_common.gicd + gicd_cpendsgir0 + i) = 0xffffffffU;
	}

	/* Disable distributor */
	*(interrupts_common.gicd + gicd_ctlr) &= ~0x3U;

	/* Set default priorities - 128 for the SGI (IRQID: 0 - 15), PPI (IRQID: 16 - 31), SPI (IRQID: 32 - 188) */
	for (i = 0; i < SIZE_INTERRUPTS; ++i) {
		interrupts_setPriority(i, DEFAULT_PRIORITY);
	}

	/* Set required configuration and CPU mask */
	for (i = SPI_FIRST_IRQID; i < SIZE_INTERRUPTS; ++i) {
		interrupts_setConf(i, (unsigned int)_interrupts_gicv2_classify(i));
		interrupts_setCPU(i, 0x1);
	}

	/* enable_secure = 1 */
	*(interrupts_common.gicd + gicd_ctlr) |= 0x3U;

	*(interrupts_common.gicc + gicc_ctlr) &= ~0x3U;

	/* Initialize CPU Interface of the gic
	 * Set the maximum priority mask and binary point */
	*(interrupts_common.gicc + gicc_bpr) = 0x3U;
	*(interrupts_common.gicc + gicc_pmr) = 0xffU;

	/* EnableGrp0 = 1; EnableGrp1 = 1; AckCtl = 1; FIQEn = 1 in secure mode
	 * EnableGrp1 = 1 in non-secure mode, other bits are ignored */
	*(interrupts_common.gicc + gicc_ctlr) = *(interrupts_common.gicc + gicc_ctlr) | 0x7U;
}


static void hal_cpuSendSGI(u32 targetFilter, u32 targetList, u32 intID)
{
	*(interrupts_common.gicd + gicd_sgir) = ((targetFilter & 0x3UL) << 24) | (targetList << 16) | (intID & 0xfU);
	hal_cpuDataSyncBarrier();
}


void hal_cpuBroadcastIPI(unsigned int intr)
{
	hal_cpuSendSGI(SGI_FLT_OTHER_CPUS, 0U, intr);
}
