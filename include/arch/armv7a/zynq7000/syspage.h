/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for Zynq-7000
 *
 * Copyright 2021 Phoenix Systems
 * Authors: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_SYSPAGE_ZYNQ7000_H_
#define _PH_SYSPAGE_ZYNQ7000_H_

typedef struct {
	int dummy;
} __attribute__((packed)) hal_syspage_t;


typedef struct {
} hal_syspage_part_t;

#endif
