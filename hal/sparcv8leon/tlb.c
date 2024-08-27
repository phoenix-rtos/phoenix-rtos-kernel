/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * TLB handling
 *
 * Copyright 2023 Phoenix Systems
 * Author: Andrzej Stalke, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include <arch/tlb.h>
#include <arch/cpu.h>

#include "srmmu.h"


void hal_tlbFlushLocal(const pmap_t *pmap)
{
	if ((pmap != NULL) && (hal_srmmuGetContext() == pmap->context)) {
		hal_srmmuFlushTLB(NULL, TLB_FLUSH_CTX);
	}
	else {
		hal_srmmuFlushTLB(NULL, TLB_FLUSH_ALL);
	}
}


void hal_tlbInvalidateLocalEntry(const pmap_t *pmap, const void *vaddr)
{
	if ((pmap != NULL) && (hal_srmmuGetContext() == pmap->context)) {
		if ((ptr_t)vaddr < VADDR_USR_MAX) {
			hal_srmmuFlushTLB(vaddr, TLB_FLUSH_L3);
		}
		else {
			hal_srmmuFlushTLB(vaddr, TLB_FLUSH_CTX);
		}
	}
	else {
		hal_srmmuFlushTLB(vaddr, TLB_FLUSH_ALL);
	}
}


int hal_tlbIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg)
{
	(void)n;
	(void)ctx;
	(void)arg;

	hal_tlbShootdown();

	return 0;
}
