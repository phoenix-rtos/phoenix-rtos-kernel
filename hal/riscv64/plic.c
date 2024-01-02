/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * RISCV64 PLIC interrupt controler driver
 *
 * Copyright 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "plic.h"

#include <board_config.h>

/* clang-format off */

/* PLIC register offsets */
#define PLIC_PRIORITY(irqn)            (0x0000 + (n) * 4)
#define PLIC_REG_PENDING(irqn)         (0x1000 + ((irqn) / 32) * 4)
#define PLIC_REG_ENABLE(context, irqn) (0x2000 + (context) * 0x80 + ((irqn) / 32) * 4)
#define PLIC_REG_THRESHOLD(context)    (0x200000 + (context) * 0x1000)
#define PLIC_REG_CLAIM(context)        (0x200004 + (context) * 0x1000)

/* clang-format on */


static struct {
	volatile u8 *regw;
} plic_common;


u32 plic_read(unsigned int reg)
{
	return (u32)(*(volatile u32 *)(plic_common.regw + reg));
}


void plic_write(unsigned int reg, u32 v)
{
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
	u32 bitshift = n % 32;

	return ((plic_read(PLIC_REG_PENDING(n)) >> bitshift) & 1);
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
	u32 bitshift = n % 32;
	u32 val;

	if (n >= PLIC_IRQ_SIZE) {
		return -1;
	}

	val = plic_read(PLIC_REG_ENABLE(context, n));

	if (enable != 0) {
		val |= (1 << bitshift);
	}
	else {
		val &= ~(1 << bitshift);
	}

	plic_write(PLIC_REG_ENABLE(context, n), val);

	return 0;
}


int plic_enableInterrupt(unsigned int hart, unsigned int n)
{
	return plic_modifyInterrupt(hart, n, 1);
}


int plic_disableInterrupt(unsigned int hart, unsigned int n)
{
	return plic_modifyInterrupt(hart, n, 0);
}


void _plic_init(void)
{
	unsigned int i;

	plic_common.regw = (void *)((u64)((1L << 39) - 1024 * 1024 * 1024 + 0x0c000000) | (u64)0xffffff8000000000);

	/* Disable and mask external interrupts */
	for (i = 1; i < PLIC_IRQ_SIZE; i++) {
		plic_priority(i, 0);
		plic_disableInterrupt(1, i);
	}

	plic_tresholdSet(1, 1);
}
