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
	u8 regw[SIZE_PAGE];
} plic_common;


typedef enum {
	plic_regTreshold, plic_regPriority, plic_regClaimComplete,
	plic_regEnable, plic_regPending
} plic_reg_t;


static inline u32 plic_read(plic_reg_t reg)
{
	return (u32)(*(u64 *)(plic_common.regw + reg));
}


static inline void plic_write(plic_reg_t reg, u32 v)
{
	*(u64 *)(plic_common.regw + reg) = v; 
	return;
}


void plic_priority(unsigned int n)
{
	plic_write(plic_regPriority + n, 0x2);
	return;
}


void plic_treshold(unsigned int priority)
{
	plic_write(plic_regTreshold, priority);
	return;
}


unsigned int plic_claim (void)
{
	return plic_read(plic_regClaimComplete);
}


int plic_complete(unsigned int n)
{
	plic_write(plic_regClaimComplete, n);
	return EOK;
}


int plic_enableInterrupt(unsigned int n, char enable)
{
	u32 reg = n / 32;
	u32 bitshift = n % 32;
	u32 w;

	if (n >= 128)
		return -ENOENT;

	w = plic_read(plic_regEnable);

	if (enable)
		w |= (1 << bitshift);
	else
		w &= ~(1 << bitshift);

	plic_write(plic_regEnable + 4 * reg, w);
	return EOK;
}


int plic_isPending (unsigned int n) {

	u32 reg = n / 32;
	u32 bitshift = n % 32;

	return ((plic_read(plic_regPending + 4 * reg) >> bitshift) & 1);
}


int _plic_init(void)
{
	return EOK;
}
