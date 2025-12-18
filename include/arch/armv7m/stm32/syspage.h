
/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for STM32 boards
 *
 * Copyright 2021 Phoenix Systems
 * Authors: Hubert Buczynski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_SYSPAGE_STM32_H_
#define _PH_SYSPAGE_STM32_H_


typedef struct {
	struct {
		struct {
			unsigned int rbar;
			unsigned int rasr;
		} table[16] __attribute__((aligned(8)));
		unsigned int map[16]; /* ((unsigned int)-1) = map is not assigned */
		unsigned int allocCnt;
	} mpu;
} hal_syspage_part_t;


typedef struct {
	unsigned int mpuType;
	unsigned int bootReason;
} __attribute__((packed)) hal_syspage_t;

#endif
