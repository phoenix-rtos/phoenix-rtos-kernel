/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for ZynqMP
 *
 * Copyright 2024 Phoenix Systems
 * Authors: Jacek Maksymowicz
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PH_SYSPAGE_ZYNQMP_H_
#define _PH_SYSPAGE_ZYNQMP_H_

typedef struct {
	long long int resetReason;
} __attribute__((packed)) hal_syspage_t;


typedef struct {
	int dummy;
} hal_syspage_part_t;

#endif
