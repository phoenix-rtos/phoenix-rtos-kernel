
/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * HAL syspage for MCXN94x
 *
 * Copyright 2021, 2022, 204 Phoenix Systems
 * Authors: Hubert Buczynski, Damian Loewnau, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_SYSPAGE_MCXN94X_H_
#define _PH_SYSPAGE_MCXN94X_H_


#ifndef MPU_MAX_REGIONS
#define MPU_MAX_REGIONS 16
#endif


typedef struct {
	struct {
		struct {
			unsigned int rbar;
			unsigned int rlar;
		} table[MPU_MAX_REGIONS] __attribute__((aligned(8)));
		unsigned int map[MPU_MAX_REGIONS]; /* ((unsigned int)-1) = map is not assigned */
		unsigned int allocCnt;
	} mpu;
} hal_syspage_prog_t;


typedef struct {
	unsigned int mpuType;
} __attribute__((packed)) hal_syspage_t;

#endif
