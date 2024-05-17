/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * TLB handling
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch/tlb.h>
#include <arch/cpu.h>

#include "hal/tlb/tlb.h"

#include "riscv64.h"


void hal_tlbFlushLocal(const pmap_t *pmap)
{
	hal_cpuFlushTLB(NULL);
}


void hal_tlbInvalidateLocalEntry(const pmap_t *pmap, const void *vaddr)
{
	hal_cpuFlushTLB(vaddr);
}


int hal_tlbIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)ctx;
	(void)arg;

	csr_clear(sip, SIP_SSIP);

	hal_tlbShootdown();

	return 0;
}
