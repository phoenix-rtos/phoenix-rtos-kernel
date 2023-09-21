/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * SPARC reference MMU routines
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include "srmmu.h"
#include "hal/sparcv8leon3/sparcv8leon3.h"


void hal_srmmuFlushTLB(void *vaddr, u8 type)
{
	addr_t addr = (addr_t)(((u32)vaddr & ~(0xfff)) | ((type & 0xf) << 8));
	hal_cpuStoreAlternate(addr, ASI_FLUSH_ALL, 0);
}


u32 hal_srmmuGetFaultSts(void)
{
	return hal_cpuLoadAlternate(MMU_FAULT_STS, ASI_MMU_REGS);
}


u32 hal_srmmuGetFaultAddr(void)
{
	return hal_cpuLoadAlternate(MMU_FAULT_ADDR, ASI_MMU_REGS);
}


void hal_srmmuSetContext(u32 ctx)
{
	hal_cpuStoreAlternate(MMU_CTX, ASI_MMU_REGS, ctx);
}


u32 hal_srmmuGetContext(void)
{
	return hal_cpuLoadAlternate(MMU_CTX, ASI_MMU_REGS);
}
