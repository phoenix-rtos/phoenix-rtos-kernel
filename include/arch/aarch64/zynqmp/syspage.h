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

#ifndef _PHOENIX_SYSPAGE_ZYNQMP_H_
#define _PHOENIX_SYSPAGE_ZYNQMP_H_

typedef struct {
	long long int resetReason;
} __attribute__((packed)) hal_syspage_t;


#endif
