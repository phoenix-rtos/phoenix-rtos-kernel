/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * GIC v3 driver
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "hal/console.h"
#include "hal/list.h"
#include "hal/interrupts.h"
#include "hal/spinlock.h"
#include "hal/string.h"

#include "include/errno.h"


#define SIZE_INTERRUPTS 126U
#define PPI_FIRST_IRQID 16U
#define SPI_FIRST_IRQID 32U


#define INTCFGR_LEVEL 0U
#define INTCFGR_EDGE  (1U << 1)


/* Cortex R52 target ID mapping
 * 0            : CPU0
 ...
 * NUM_CPUS - 1 : CPU(NUM_CPUS - 1)
 * NUM_CPUS     : Export port
 */

/* clang-format off */

/* GIC memory map */
enum {
	gicd_base         = 0,      /* Distributor registers                              : 0x000000-0x00ffff */
	gicr_ctlr_tgt0    = 262144, /* Redistributor registers for Control target 0       : 0x100000-0x10ffff */
	gicr_sgi_ppi_tgt0 = 278528, /* Redistributor registers for SGIs and PPIs target 0 : 0x110000-0x11ffff */
	gicr_ctlr_tgt1    = 294912, /* Redistributor registers for Control target 1       : 0x120000-0x12ffff */
	gicr_sgi_ppi_tgt1 = 311296, /* Redistributor registers for SGIs and PPIs target 1 : 0x130000-0x13ffff */
	gicr_ctlr_tgt2    = 327680, /* Redistributor registers for Control target 2       : 0x140000-0x14ffff */
	gicr_sgi_ppi_tgt2 = 344064, /* Redistributor registers for SGIs and PPIs target 2 : 0x150000-0x15ffff */
	gicr_ctlr_tgt3    = 360448, /* Redistributor registers for Control target 3       : 0x160000-0x16ffff */
	gicr_sgi_ppi_tgt3 = 376832, /* Redistributor registers for SGIs and PPIs target 3 : 0x170000-0x17ffff */
	gicr_ctlr_tgt4    = 393216, /* Redistributor registers for Control target 4       : 0x180000-0x18ffff */
	gicr_sgi_ppi_tgt4 = 409600, /* Redistributor registers for SGIs and PPIs target 4 : 0x190000-0x19ffff */
};

/* Distributor register map */
enum {
	gicd_ctlr        = gicd_base + 0,     /* Distributor Control Register                    : 0x0000 */
	gicd_typer       = gicd_base + 1,     /* Interrupt Controller Type Register              : 0x0004 */
	gicd_iidr        = gicd_base + 2,     /* Distributor Implementer Identification Register : 0x0008 */
	gicd_igroupr1    = gicd_base + 33,    /* Interrupt Group Registers 1-30                  : 0x0084 - 0x00f8 */
	gicd_isenabler1  = gicd_base + 65,    /* Interrupt Set-Enable Registers 1-30             : 0x0104 - 0x0178 */
	gicd_icenabler1  = gicd_base + 97,    /* Interrupt Clear-Enable Registers 1-30           : 0x0184 - 0x01f8 */
	gicd_ispendr1    = gicd_base + 129,   /* Interrupt Set-Pending Registers 1-30            : 0x0204 - 0x0278 */
	gicd_icpendr1    = gicd_base + 161,   /* Interrupt Clear-Pending Registers 1-30          : 0x0284 - 0x02f8 */
	gicd_isactiver1  = gicd_base + 193,   /* Interrupt Set-Active Registers 1-30             : 0x0304 - 0x0378 */
	gicd_icactiver1  = gicd_base + 225,   /* Interrupt Clear-Active Registers 1-30           : 0x0384 - 0x03f8 */
	gicd_ipriorityr8 = gicd_base + 264,   /* Interrupt Priority Registers 8-247              : 0x0420 - 0x07df */
	gicd_icfgr2      = gicd_base + 770,   /* Interrupt Configuration Registers 2-61          : 0x0c08 - 0x0cf4 */
	gicd_irouter32   = gicd_base + 6208,  /* Interrupt Routing Registers 32-991              : 0x6100 - 0x7ef8 */
	gicd_pidr0       = gicd_base + 16376, /* Identification Registers 0-7                    : 0xffe0 - 0xffdc */
	gicd_cidr0       = gicd_base + 16380  /* Component Identification Registers 0-3          : 0xfff0 - 0xfffc */
};


/* Redistributor register map */
enum {
	gicr_ctlr        = gicr_ctlr_tgt0 + 0,      /* Redistributor Control Register                    : 0x0000 */
	gicr_iidr        = gicr_ctlr_tgt0 + 1,      /* Redistributor Implementer Identification Register : 0x0004 */
	gicr_typer       = gicr_ctlr_tgt0 + 2,      /* Redistributor Type Register                       : 0x0008 - 0x000c */
	gicr_waker       = gicr_ctlr_tgt0 + 5,      /* Redistributor Wake Register                       : 0x0014 */
	gicr_igroupr0    = gicr_sgi_ppi_tgt0 + 32,  /* Interrupt Group Register 0                        : 0x0080 */
	gicr_isenabler0  = gicr_sgi_ppi_tgt0 + 64,  /* Interrupt Set-Enable Register 0                   : 0x0100 */
	gicr_icenabler0  = gicr_sgi_ppi_tgt0 + 96,  /* Interrupt Clear-Enable Register 0                 : 0x0180 */
	gicr_ispendr0    = gicr_sgi_ppi_tgt0 + 128, /* Interrupt Set-Pending Register 0                  : 0x0200 */
	gicr_icpendr0    = gicr_sgi_ppi_tgt0 + 160, /* Interrupt Clear-Pending Register 0                : 0x0280 */
	gicr_isactiver0  = gicr_sgi_ppi_tgt0 + 192, /* Interrupt Set-Active Register 0                   : 0x0300 */
	gicr_icactiver0  = gicr_sgi_ppi_tgt0 + 224, /* Interrupt Clear-Active Register 0                 : 0x0380 */
	gicr_ipriorityr0 = gicr_sgi_ppi_tgt0 + 256, /* Interrupt Priority Register 0-7                   : 0x0400 - 0x41c */
	gicr_icfgr0      = gicr_sgi_ppi_tgt0 + 768, /* Interrupt Configuration Register 0                : 0x0c00 */
	gicr_icfgr1      = gicr_sgi_ppi_tgt0 + 769, /* Interrupt Configuration Register 1                : 0x0c04 */
	gicr_pidr0       = gicr_ctlr_tgt0 + 16376,  /* Redistributor Identification Regs 0-7             : 0xffe0 - 0xffdc */
	gicr_cidr0       = gicr_ctlr_tgt0 + 16380   /* Redistributor Component Identification Regs 0-3   : 0xfff0 - 0xfffc */
};

/* clang-format on */


static struct {
	volatile u32 *gic;
	spinlock_t lock;
	intr_handler_t *handlers[SIZE_INTERRUPTS];
} interrupts_common;


int threads_schedule(unsigned int n, cpu_context_t *context, void *arg);


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
static u32 gic_acknowledge(void)
{
	u32 irqn;

	/* Read the Interrupt Acknowledge Register (Group 1) */
	/* clang-format off */
	__asm__ volatile (
		"mrc p15, 0, %0, c12, c12, 0"
		: "=r"(irqn)
	);
	/* clang-format on */

	return irqn & 0x3ffU;
}


static void gic_EOI(u32 irqn)
{
	/* Update End of Interrupt register */
	/* clang-format off */
	__asm__ volatile (
		"mcr p15, 0, %0, c12, c12, 1"
		: : "r"(irqn)
	);
	/* clang-format on */
}


/* parasoft-begin-suppress MISRAC2012-RULE_2_2 MISRAC2012-RULE_8_4 "Function is used externally within assembler code" */
int interrupts_dispatch(unsigned int n, cpu_context_t *ctx)
{
	unsigned int reschedule = 0;
	intr_handler_t *h;
	spinlock_ctx_t sc;

	n = gic_acknowledge();

	if (n >= SIZE_INTERRUPTS) {
		/* Spurious interrupt */
		return 0;
	}

	hal_spinlockSet(&interrupts_common.lock, &sc);

	h = interrupts_common.handlers[n];
	if (h != NULL) {
		do {
			hal_cpuSetGot(h->got);
			reschedule |= (unsigned int)h->f(n, NULL, h->data);
			h = h->next;
		} while (h != interrupts_common.handlers[n]);
	}

	if (reschedule != 0U) {
		(void)threads_schedule(n, ctx, NULL);
	}

	hal_spinlockClear(&interrupts_common.lock, &sc);

	gic_EOI(n);

	return (int)reschedule;
}


static void gic_waitRWP(int reg)
{
	/* Wait for register write to complete */
	if (reg == gicd_ctlr) {
		while ((*(interrupts_common.gic + reg) & (1UL << 31)) != 0U) { }
	}
	else if (reg == gicr_ctlr) {
		while ((*(interrupts_common.gic + reg) & (1U << 3)) != 0U) { }
	}
	else {
		/* No action required*/
	}
}


void hal_interruptsEnable(unsigned int irqn)
{
	if (irqn < SPI_FIRST_IRQID) {
		*(interrupts_common.gic + gicr_isenabler0) = 1UL << irqn;
	}
	else {
		*(interrupts_common.gic + gicd_isenabler1 + ((irqn - SPI_FIRST_IRQID) / 32U)) = 1UL << (irqn & 0x1fU);
	}
}


void hal_interruptsDisable(unsigned int irqn)
{
	if (irqn < SPI_FIRST_IRQID) {
		*(interrupts_common.gic + gicr_icenabler0) = 1UL << irqn;
		gic_waitRWP(gicr_ctlr);
	}
	else {
		*(interrupts_common.gic + gicd_icenabler1 + ((irqn - SPI_FIRST_IRQID) / 32U)) = 1UL << (irqn & 0x1fU);
		gic_waitRWP(gicd_ctlr);
	}
}


static void interrupts_setConfig(unsigned int irqn, u8 conf)
{
	u32 mask;

	if (irqn < PPI_FIRST_IRQID) {
		mask = *(interrupts_common.gic + gicr_icfgr0 + (irqn / 16U)) & ~(0x3U << ((irqn & 0xfU) * 2U));

		*(interrupts_common.gic + gicr_icfgr0 + (irqn / 16U)) = mask | (((u32)conf & 0x3U) << ((irqn & 0xfU) * 2U));
	}
	else if (irqn < SPI_FIRST_IRQID) {
		mask = *(interrupts_common.gic + gicr_icfgr1 + ((irqn - PPI_FIRST_IRQID) / 16U)) & ~(0x3U << ((irqn & 0xfU) * 2U));

		*(interrupts_common.gic + gicr_icfgr1 + ((irqn - PPI_FIRST_IRQID) / 16U)) = mask | (((u32)conf & 0x3U) << ((irqn & 0xfU) * 2U));
	}
	else {
		mask = *(interrupts_common.gic + gicd_icfgr2 + ((irqn - SPI_FIRST_IRQID) / 16U)) & ~(0x3U << ((irqn & 0xfU) * 2U));

		*(interrupts_common.gic + gicd_icfgr2 + ((irqn - SPI_FIRST_IRQID) >> 4)) = mask | (((u32)conf & 0x3U) << ((irqn & 0xfU) * 2U));
	}
}


static void interrupts_setPriority(unsigned int irqn, u32 priority)
{
	u32 mask;

	if (irqn < SPI_FIRST_IRQID) {
		mask = *(interrupts_common.gic + gicr_ipriorityr0 + (irqn / 4U)) & ~(0xffU << ((irqn & 0x3U) * 8U));

		*(interrupts_common.gic + gicr_ipriorityr0 + (irqn / 4U)) = mask | ((priority & 0xffU) << ((irqn & 0x3U) * 8U));
	}
	else {
		mask = *(interrupts_common.gic + gicd_ipriorityr8 + ((irqn - SPI_FIRST_IRQID) / 4U)) & ~(0xffU << ((irqn & 0x3U) * 8U));

		*(interrupts_common.gic + gicd_ipriorityr8 + ((irqn - SPI_FIRST_IRQID) / 4U)) = mask | ((priority & 0xffU) << ((irqn & 0x3U) * 8U));
	}
}


static void interrupts_setGroup(unsigned int irqn, u32 group)
{
	if (irqn < SPI_FIRST_IRQID) {
		if (group == 0U) {
			*(interrupts_common.gic + gicr_igroupr0) &= ~(1U << irqn);
		}
		else {
			*(interrupts_common.gic + gicr_igroupr0) |= 1UL << irqn;
		}
	}
	else {
		if (group == 0U) {
			*(interrupts_common.gic + gicd_igroupr1 + ((irqn - SPI_FIRST_IRQID) / 32U)) &= ~(1U << (irqn & 0x1fU));
		}
		else {
			*(interrupts_common.gic + gicd_igroupr1 + ((irqn - SPI_FIRST_IRQID) / 32U)) |= 1UL << (irqn & 0x1fU);
		}
	}
}


int hal_interruptsSetHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if ((h == NULL) || (h->n >= SIZE_INTERRUPTS)) {
		return -EINVAL;
	}

	h->got = hal_cpuGetGot();

	hal_spinlockSet(&interrupts_common.lock, &sc);

	HAL_LIST_ADD(&interrupts_common.handlers[h->n], h);

	interrupts_setGroup(h->n, 1);
	interrupts_setPriority(h->n, 0xa);
	interrupts_setConfig(h->n, INTCFGR_EDGE);
	hal_interruptsEnable(h->n);

	hal_spinlockClear(&interrupts_common.lock, &sc);

	return 0;
}


int hal_interruptsDeleteHandler(intr_handler_t *h)
{
	spinlock_ctx_t sc;

	if ((h == NULL) || (h->n >= SIZE_INTERRUPTS)) {
		return -EINVAL;
	}

	hal_spinlockSet(&interrupts_common.lock, &sc);

	HAL_LIST_REMOVE(&interrupts_common.handlers[h->n], h);
	hal_interruptsDisable(h->n);

	hal_spinlockClear(&interrupts_common.lock, &sc);

	return 0;
}


char *hal_interruptsFeatures(char *features, unsigned int len)
{
	(void)hal_strncpy(features, "Using GICv3 interrupt controller", len);
	features[len - 1U] = '\0';

	return features;
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
void _hal_interruptsInit(void)
{
	u32 i, val;

	hal_spinlockCreate(&interrupts_common.lock, "interrupts");

	/* Read GIC base address (IMP_CBAR) */
	/* clang-format off */
	__asm__ volatile (
		"mrc p15, 1, %0, c15, c3, 0"
		: "=r"(interrupts_common.gic)
	);
	/* clang-format on */

	/* Cortex-R52:
	 * - Only supports GICv3 accesses
	 * - No security support
	 */

	/*------------- Configure interrupt controller -------------*/

	/* Enable Group 1 interrupts */
	*(interrupts_common.gic + gicd_ctlr) = (1U << 1);
	gic_waitRWP(gicd_ctlr);

	/* Clear ProcessorSleep */
	*(interrupts_common.gic + gicr_waker) &= ~(1U << 1);

	/* Wait for ChildrenAsleep to become 0 */
	while ((*(interrupts_common.gic + gicr_waker) & (1UL << 2)) != 0U) { }

	/* ICC_SRE SRE bit fixed 1, no need to write */

	/* Set priority mask (ICC_PMR register) */
	/* clang-format off */
	__asm__ volatile (
		"mcr p15, 0, %0, c4, c6, 0"
		: : "r"(0xff)
	);

	/* Setup ICC_CTLR register */
	__asm__ volatile (
		"mrc p15, 0, %0, c12, c12, 4"
		: "=r"(val)
	);

	/* Set EOI mode 0 */
	val &= ~(1U << 1);

	__asm__ volatile (
		"mcr p15, 0, %0, c12, c12, 4"
		: : "r"(val)
	);

	/* Enable Group 1 interrupts (ICC_IGRPEN1 register) */
	__asm__ volatile (
		"mcr p15, 0, %0, c12, c12, 7"
		: : "r"(1)
	);
	/* clang-format on */

	/*------------- Configure interrupt sources -------------*/

	for (i = SPI_FIRST_IRQID; i < SIZE_INTERRUPTS; i++) {
		hal_interruptsDisable(i);
	}

	gic_waitRWP(gicd_ctlr);

	for (i = SPI_FIRST_IRQID; i < SIZE_INTERRUPTS; i += 4U) {
		/* Set default priority (SPIs) */
		*(interrupts_common.gic + gicd_ipriorityr8 + ((i - SPI_FIRST_IRQID) / 4U)) = 0xa0a0a0a0U;
	}

	for (i = 0; i < SPI_FIRST_IRQID; i += 4U) {
		/* Set default priority (SGIs/PPIs) */
		*(interrupts_common.gic + gicr_ipriorityr0 + (i / 4U)) = 0xa0a0a0a0U;
	}

	/* Disable PPIs/SGIs */
	*(interrupts_common.gic + gicr_icenabler0) = 0xffffffffU;
	gic_waitRWP(gicr_ctlr);
}
