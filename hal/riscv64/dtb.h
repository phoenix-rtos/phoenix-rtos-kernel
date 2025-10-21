/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * DTB parser
 *
 * Copyright 2018, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_DTB_H_
#define _PH_HAL_DTB_H_

#include "hal/cpu.h"

#define ntoh16(x) ((((x) << 8) & 0xff00U) | (((x) >> 8) & 0xffU))
#define ntoh32(x) ((ntoh16(x) << 16) | ntoh16((x) >> 16))
#define ntoh64(x) ((((u64)ntoh32(x)) << 32) | (((u64)ntoh32(x)) >> 32))


void dtb_save(void *dtb);


void dtb_parse(void);


void dtb_getSystem(char **model, char **compatible);


int dtb_getCPU(unsigned int n, char **compatible, u32 *clock, char **isa, char **mmu);


void dtb_getMemory(u8 **reg, size_t *nreg);


int dtb_getPLIC(void);


void dtb_getReservedMemory(u64 **reg);


void dtb_getDTBArea(u64 *dtb, u32 *dtbsz);


void _dtb_init(void);


#endif
