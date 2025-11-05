/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for riscv64
 *
 * Copyright 2021 Phoenix Systems
 * Authors: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_SYSPAGE_RISCV64_H_
#define _PH_SYSPAGE_RISCV64_H_


typedef struct {
	unsigned int boothartId;
} __attribute__((packed)) hal_syspage_t;

#endif
