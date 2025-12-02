
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

#ifndef _PH_SYSPAGE_IMXRT_H_
#define _PH_SYSPAGE_IMXRT_H_


#ifndef MPU_MAX_REGIONS
#define MPU_MAX_REGIONS 16
#endif


typedef struct {
	struct {
		struct {
			unsigned int rbar;
			unsigned int rasr;
		} table[MPU_MAX_REGIONS] __attribute__((aligned(8)));
		unsigned int map[MPU_MAX_REGIONS]; /* ((unsigned int)-1) = map is not assigned */
		unsigned int allocCnt;
	} mpu;
} hal_syspage_prog_t;


typedef struct {
	unsigned int mpuType;
	unsigned int bootReason;
} __attribute__((packed)) hal_syspage_t;

#endif
