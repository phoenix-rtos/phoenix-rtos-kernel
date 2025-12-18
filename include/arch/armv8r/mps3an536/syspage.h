/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for MPS3-AN536
 *
 * Copyright 2024 Phoenix Systems
 * Authors: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_SYSPAGE_MPS3AN536_H_
#define _PH_SYSPAGE_MPS3AN536_H_


typedef struct {
	int dummy;
} __attribute__((packed)) hal_syspage_t;


typedef struct {
} hal_syspage_part_t;

#endif
