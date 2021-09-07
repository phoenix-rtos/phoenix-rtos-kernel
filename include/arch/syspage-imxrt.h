
/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for i.MX RT boards
 *
 * Copyright 2021 Phoenix Systems
 * Authors: Hubert Buczynski, Gerard Swiderski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_SYSPAGE_IMXRT_H_
#define _PHOENIX_SYSPAGE_IMXRT_H_


typedef struct {
	struct {
		u32 type;
		u32 allocCnt;
		struct {
			u32 rbar;
			u32 rasr;
		} table[16] __attribute__((aligned(8)));
		u32 map[16]; /* ((u32)-1) = map is not assigned */
	} __attribute__((packed)) mpu;
} __attribute__((packed)) hal_syspage_t;

#endif
