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

#include "include/errno.h"


struct {
	volatile u8 *regw;
	unsigned int baseEnable;
	unsigned int baseContext;
} plic_common;


static inline u32 plic_read(unsigned int reg)
{
	return (u32)(*(volatile u32 *)(plic_common.regw + reg));
}


static inline void plic_write(unsigned int reg, u32 v)
{
	*(volatile u32 *)(plic_common.regw + reg) = v; 
	return;
}


void plic_priority(unsigned int n, unsigned int priority)
{
	plic_write(n * 4, priority);
	return;
}


u32 plic_priorityGet(unsigned int n)
{
	return plic_read(n * 4);
}


int plic_isPending(unsigned int n)
{
	u32 reg = n / 32;
	u32 bitshift = n % 32;

	return ((plic_read(0x1000 + 4 * reg) >> bitshift) & 1);
}


void plic_tresholdSet(unsigned int hart, unsigned int priority)
{
	plic_write(plic_common.baseContext + hart * 0x1000, priority);
	return;
}


u32 plic_tresholdGet(unsigned int hart)
{
	return plic_read(plic_common.baseContext + hart * 0x1000);
}


unsigned int plic_claim(unsigned int hart)
{
	return plic_read(plic_common.baseContext + hart * 0x1000 + 4);
}


int plic_complete(unsigned int hart, unsigned int n)
{
	plic_write(plic_common.baseContext + hart * 0x1000 + 4, n);
	return EOK;
}


int plic_enableInterrupt(unsigned int hart, unsigned int n, char enable)
{
	u32 reg = n / 32;
	u32 bitshift = n % 32;
	u32 w;

	if (n >= 128)
		return -ENOENT;

	w = plic_read(plic_common.baseEnable + hart * 0x80);

	if (enable)
		w |= (1 << bitshift);
	else
		w &= ~(1 << bitshift);

	plic_write(plic_common.baseEnable + hart * 0x80 + 4 * reg, w);

	return EOK;
}


int _plic_init(void)
{
	unsigned int i;

	plic_common.baseEnable = 0x2000;
	plic_common.baseContext = 0x200000;

	plic_common.regw = (void *)((u64)((1L << 39) - 1024 * 1024 * 1024 + 0x0c000000)| (u64)0xffffff8000000000);

	/* Disable and mask external interrupts */
	for (i = 1; i < 127; i++) {
		plic_priority(i, 0);
		plic_enableInterrupt(1, i, 0);
	}

	plic_tresholdSet(1, 1);

	return EOK;
}
