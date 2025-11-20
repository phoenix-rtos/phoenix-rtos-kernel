/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for MPS3-AN536
 *
 * Copyright 2024 Phoenix Systems
 * Authors: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_SYSPAGE_MPS3AN536_H_
#define _PHOENIX_SYSPAGE_MPS3AN536_H_

typedef struct {
	unsigned int allocCnt;
	struct {
		unsigned int rbar;
		unsigned int rlar;
	} table[16] __attribute__((aligned(8)));
	unsigned int map[16]; /* ((unsigned int)-1) = map is not assigned */
} hal_syspage_prog_t;


typedef struct {
	struct {
		unsigned int type;
		unsigned int allocCnt;
		unsigned int mair[2];
		struct {
			unsigned int rbar;
			unsigned int rlar;
		} table[16] __attribute__((aligned(8)));
		unsigned int map[16]; /* ((unsigned int)-1) = map is not assigned */
	} __attribute__((packed)) mpu;
	unsigned int bootReason;
} __attribute__((packed)) hal_syspage_t;


#endif
