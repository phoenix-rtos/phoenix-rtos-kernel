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

#ifndef _PH_SYSPAGE_ARMV7R_ZYNQMP_H_
#define _PH_SYSPAGE_ARMV7R_ZYNQMP_H_


typedef struct {
	struct {
		struct {
			unsigned int rbar;
			unsigned int rasr;
		} table[16] __attribute__((aligned(8)));
		unsigned int map[16]; /* ((unsigned int)-1) = map is not assigned */
		unsigned int allocCnt;
	} __attribute__((packed)) mpu;
} __attribute__((packed)) hal_syspage_part_t;


typedef struct {
	int resetReason;
	unsigned int mpuType;
} __attribute__((packed)) hal_syspage_t;


#endif
