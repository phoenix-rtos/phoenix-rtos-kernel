/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * TLB handling
 *
 * Copyright 2023, 2024 Phoenix Systems
 * Author: Andrzej Stalke, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#ifndef _HAL_RISCV_TLB_H_
#define _HAL_RISCV_TLB_H_


#include "cpu.h"
#include "pmap.h"


void hal_tlbFlushLocal(const pmap_t *pmap);


void hal_tlbInvalidateLocalEntry(const pmap_t *pmap, const void *vaddr);


int hal_tlbIrqHandler(unsigned int n, cpu_context_t *ctx, void *arg);


#endif
