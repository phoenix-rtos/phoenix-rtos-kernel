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


#ifndef _HAL_RISCV64_TLB_H_
#define _HAL_RISCV64_TLB_H_

#include <arch/pmap.h>


void hal_tlbFlushLocal(const pmap_t *pmap);


void hal_tlbInvalidateLocalEntry(const pmap_t *pmap, const void *vaddr);


void hal_tlbIrqHandler(const pmap_t *pmap, const void *vaddr, unsigned long size);


#endif
