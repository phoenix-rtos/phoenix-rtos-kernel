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
	int resetReason;
} __attribute__((packed)) hal_syspage_t;


#endif
