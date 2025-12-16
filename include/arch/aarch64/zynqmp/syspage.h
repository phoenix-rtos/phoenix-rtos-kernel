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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_SYSPAGE_ZYNQMP_H_
#define _PH_SYSPAGE_ZYNQMP_H_

typedef struct {
	long long int resetReason;
} __attribute__((packed)) hal_syspage_t;


typedef struct {
} hal_syspage_prog_t;

#endif
