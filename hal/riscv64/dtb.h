/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * DTB parser
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_DTB_H_

#include "hal/cpu.h"

#define ntoh16(x) (((x << 8) & 0xff00) | ((x >> 8) & 0xff))
#define ntoh32(x) ((ntoh16(x) << 16) | ntoh16(x >> 16))
#define ntoh64(x) ((ntoh32(x) << 32) | ntoh32(x >> 32))


extern void dtb_parse(void *arg, void *dtb);


extern const void dtb_getSystem(char **model, char **compatible);


extern int dtb_getCPU(unsigned int n, char **compatible, u32 *clock, char **isa, char **mmu);


extern void dtb_getMemory(u64 **reg, size_t *nreg);


extern int dtb_getPLIC(void);


extern void dtb_getReservedMemory(u64 **reg);


extern void dtb_getDTBArea(u64 *dtb, u32 *dtbsz);


#endif
