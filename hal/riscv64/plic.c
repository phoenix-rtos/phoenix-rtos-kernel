/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * RISCV64 PLIC interrupt controler driver
 *
 * Copyright 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch/pmap.h>

#include "plic.h"
#include "hal/cpu.h"
#include "riscv64.h"

#include <board_config.h>

/* clang-format off */

/* PLIC register offsets */
#define PLIC_PRIORITY(irqn)            (0x0000U + (irqn) * 4U)
#define PLIC_REG_PENDING(irqn)         (0x1000U + ((irqn) / 32U) * 4U)
#define PLIC_REG_ENABLE(context, irqn) (0x2000U + (context) * 0x80U + ((irqn) / 32U) * 4U)
#define PLIC_REG_THRESHOLD(context)    (0x200000U + (context) * 0x1000U)
#define PLIC_REG_CLAIM(context)        (0x200004U + (context) * 0x1000U)

/* clang-format on */

/* Value calculated from MAX_CPU_COUNT (2 contexts/cpu), TODO(?): get from DTB */
#define PLIC_SIZE PLIC_REG_THRESHOLD(2U * MAX_CPU_COUNT)


static struct {
	volatile u8 *regw;
} plic_common;


u32 plic_read(unsigned int reg)
{
	u32 ret = *(volatile u32 *)(plic_common.regw + reg);
	RISCV_FENCE(i, r);
	return ret;
}


void plic_write(unsigned int reg, u32 v)
{
	RISCV_FENCE(w, o);
	*(volatile u32 *)(plic_common.regw + reg) = v;
}


void plic_priority(unsigned int n, unsigned int priority)
{
	plic_write(PLIC_PRIORITY(n), priority);
}


u32 plic_priorityGet(unsigned int n)
{
	return plic_read(PLIC_PRIORITY(n));
}


int plic_isPending(unsigned int n)
{
	u32 bitshift = n % 32U;

	return (int)(unsigned int)(((plic_read(PLIC_REG_PENDING(n)) >> bitshift) & 1U));
}


void plic_tresholdSet(unsigned int context, unsigned int priority)
{
	plic_write(PLIC_REG_THRESHOLD(context), priority);
}


u32 plic_tresholdGet(unsigned int context)
{
	return plic_read(PLIC_REG_THRESHOLD(context));
}


unsigned int plic_claim(unsigned int context)
{
	return plic_read(PLIC_REG_CLAIM(context));
}


void plic_complete(unsigned int context, unsigned int n)
{
	plic_write(PLIC_REG_CLAIM(context), n);
}


static int plic_modifyInterrupt(unsigned int context, unsigned int n, char enable)
{
	u32 bitshift = n % 32U;
	u32 val;

	if (n >= (unsigned int)PLIC_IRQ_SIZE) {
		return -1;
	}

	val = plic_read(PLIC_REG_ENABLE(context, n));

	if (enable != '\0') {
		val |= ((u32)1 << bitshift);
	}
	else {
		val &= ~(1U << bitshift);
	}

	plic_write(PLIC_REG_ENABLE(context, n), val);

	return 0;
}


int plic_enableInterrupt(unsigned int context, unsigned int n)
{
	return plic_modifyInterrupt(context, n, (char)1);
}


int plic_disableInterrupt(unsigned int context, unsigned int n)
{
	return plic_modifyInterrupt(context, n, '\0');
}


void plic_initCore(void)
{
	unsigned int i;
	for (i = 1U; i < (unsigned int)PLIC_IRQ_SIZE; i++) {
		(void)plic_disableInterrupt(PLIC_SCONTEXT(hal_cpuGetID()), i);
	}
	plic_tresholdSet(PLIC_SCONTEXT(hal_cpuGetID()), 1);
}


void plic_init(void)
{
	unsigned int i;

	plic_common.regw = _pmap_halMapDevice(PAGE_ALIGN(PLIC_BASE), PAGE_OFFS(PLIC_BASE), PLIC_SIZE);

	/* Disable and mask external interrupts */
	for (i = 1; i < (unsigned int)PLIC_IRQ_SIZE; i++) {
		plic_priority(i, 0);
	}
}
