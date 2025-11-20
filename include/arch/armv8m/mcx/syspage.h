
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

#ifndef _PHOENIX_SYSPAGE_MCXN94X_H_
#define _PHOENIX_SYSPAGE_MCXN94X_H_
typedef struct {
	unsigned int allocCnt;
	struct _mpu_table_t {
		unsigned int rbar;
		unsigned int rlar;
	} table[16] __attribute__((aligned(8)));
	unsigned int map[16]; /* ((unsigned int)-1) = map is not assigned */
} hal_syspage_prog_t;


typedef struct {
	struct {
		unsigned int type;
		unsigned int allocCnt;
		struct {
			unsigned int rbar;
			unsigned int rlar;
		} table[16] __attribute__((aligned(8)));
		unsigned int map[16]; /* ((unsigned int)-1) = map is not assigned */
	} __attribute__((packed)) mpu;
} __attribute__((packed)) hal_syspage_t;

#endif
