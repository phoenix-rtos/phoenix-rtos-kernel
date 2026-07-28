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
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_SYSPAGE_ARMV7R_ZYNQMP_H_
#define _PH_SYSPAGE_ARMV7R_ZYNQMP_H_


typedef struct {
	int resetReason;
	struct {
		unsigned int type;
		unsigned int allocCnt;
		struct {
			unsigned int rbar;
			unsigned int rasr;
		} table[16] __attribute__((aligned(8)));
		unsigned int map[16]; /* ((unsigned int)-1) = map is not assigned */
	} __attribute__((packed)) mpu;
} __attribute__((packed)) hal_syspage_t;


#endif
