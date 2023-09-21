/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Configuration file for sparcv8leon3-gr712rc
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_CONFIG_H_
#define _HAL_CONFIG_H_


#ifndef __ASSEMBLY__


#include "gr712rc.h"

#include "include/arch/syspage-sparcv8leon3.h"
#include "include/syspage.h"

#define HAL_NAME_PLATFORM "SPARCv8 LEON3-GR712RC"

#define SIZE_EXTEND_BSS (2 * SIZE_PAGE)

#define ADDR_SRAM 0x40000000
#define SIZE_SRAM (128 * 1024 * 1024) /* 128 MB */

extern unsigned int _end;

#define VADDR_PERIPH_BASE (void *)(((u32)(&_end) + 2 * SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))

/* Peripherals' offsets */

#define PAGE_OFFS_CONSOLE  0x100
#define PAGE_OFFS_INT_CTRL 0x200
#define PAGE_OFFS_GPTIMER0 0x300
#define PAGE_OFFS_CGU      0xd00


#endif /* __ASSEMBLY__ */


#define NWINDOWS 8
#define NUM_CPUS 2


#endif
