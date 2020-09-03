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

#include "hal.h"
#include "pmap.h"


#include "../../include/errno.h"


struct {
	u8 *regw;
	unsigned int baseEnable;
	unsigned int baseContext;
} plic_common;


static inline u32 plic_read(unsigned int reg)
{
	return (u32)(*(u32 *)(plic_common.regw + reg));
}


static inline void plic_write(unsigned int reg, u32 v)
{
	*(u32 *)(plic_common.regw + reg) = v; 
	return;
}


void plic_priority(unsigned int n, unsigned int priority)
{
	plic_write(4 + n * 4, priority);
	return;
}


int plic_isPending(unsigned int n)
{
	u32 reg = n / 32;
	u32 bitshift = n % 32;

	return ((plic_read(0x1000 + 4 * reg) >> bitshift) & 1);
}


void plic_treshold(unsigned int hart, unsigned int priority)
{
	plic_write(plic_common.baseContext + hart * 0x1000, priority);
	return;
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
	plic_common.baseEnable = 0x2000;
	plic_common.baseContext = 0x200000;
	return EOK;
}
