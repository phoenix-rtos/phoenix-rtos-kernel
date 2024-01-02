/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * SBI routines (RISCV64)
 *
 * Copyright 2018, 2020, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_SBI_H_
#define _HAL_SBI_H_

#include <arch/types.h>


typedef struct {
	long error;
	long value;
} sbiret_t;


/* Legacy SBI v0.1 calls */


long hal_sbiPutchar(int ch);


long hal_sbiGetchar(void);


/* SBI v0.2+ calls */


sbiret_t hal_sbiGetSpecVersion(void);


sbiret_t hal_sbiProbeExtension(long extid);


void hal_sbiSetTimer(u64 stime);


void _hal_sbiInit(void);


#endif
