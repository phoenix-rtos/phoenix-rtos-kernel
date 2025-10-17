/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for ZynqMP
 *
 * Copyright 2025 Phoenix Systems
 * Authors: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_SYSPAGE_ARMV7R_ZYNQMP_H_
#define _PHOENIX_SYSPAGE_ARMV7R_ZYNQMP_H_


typedef struct {
	unsigned int allocCnt;
	struct {
		unsigned int rbar;
		unsigned int rasr;
	} table[16] __attribute__((aligned(8)));
	unsigned int map[16]; /* ((unsigned int)-1) = map is not assigned */
} hal_syspage_prog_t;


typedef struct {
	int resetReason;
	struct {
		unsigned int type;
	} __attribute__((packed)) mpu;
} __attribute__((packed)) hal_syspage_t;


#endif
