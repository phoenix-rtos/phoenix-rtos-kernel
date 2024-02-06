/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for sparcv8leon3
 *
 * Copyright 2023 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_SYSPAGE_LEON3_H_
#define _PHOENIX_SYSPAGE_LEON3_H_


typedef struct {
	int dummy;
} __attribute__((packed)) hal_syspage_t;

#endif
