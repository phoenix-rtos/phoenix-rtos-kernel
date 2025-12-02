/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for i.MX 6ULL
 *
 * Copyright 2021 Phoenix Systems
 * Authors: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_SYSPAGE_IMX6ULL_H_
#define _PH_SYSPAGE_IMX6ULL_H_


typedef struct {
	int dummy;
} __attribute__((packed)) hal_syspage_t;


typedef struct {
} hal_syspage_prog_t;

#endif
