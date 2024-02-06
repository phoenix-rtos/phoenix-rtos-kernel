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

#ifndef _PHOENIX_SYSPAGE_RISCV64_H_
#define _PHOENIX_SYSPAGE_RISCV64_H_


typedef struct {
	int dummy;
} __attribute__((packed)) hal_syspage_t;

#endif
