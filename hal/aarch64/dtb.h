/*
 * Phoenix-RTOS
 *
 * Operating system loader
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

#ifndef _HAL_DTB_H_

#include "hal/types.h"

#define ntoh16(x) (((x << 8) & 0xff00) | ((x >> 8) & 0xff))
#define ntoh32(x) ((ntoh16(x) << 16) | ntoh16(x >> 16))
#define ntoh64(x) ((ntoh32(x) << 32) | ntoh32(x >> 32))

typedef struct {
	addr_t start;
	addr_t end;
} dtb_memBank_t;


typedef struct {
	addr_t base;
	int intr;
} dtb_serial_t;


void dtb_getSystem(char **model, char **compatible);


int dtb_getCPU(unsigned int n, char **compatible, u32 *clock);


void dtb_getMemory(dtb_memBank_t **banks, size_t *nBanks);


void dtb_getGIC(addr_t *gicc, addr_t *gicd);


void dtb_getSerials(dtb_serial_t **serials, size_t *nSerials);


void _dtb_init(addr_t dtbPhys);


#endif
