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

#include "hal/types.h"


typedef struct {
	long error;
	long value;
} sbiret_t;


/* Reset types */
#define SBI_RESET_TYPE_SHUTDOWN 0x0
#define SBI_RESET_TYPE_COLD     0x1
#define SBI_RESET_TYPE_WARM     0x2

/* Reset reason */
#define SBI_RESET_REASON_NONE    0x0
#define SBI_RESET_REASON_SYSFAIL 0x1


/* Legacy SBI v0.1 calls */


long hal_sbiPutchar(int ch);


long hal_sbiGetchar(void);


/* SBI v0.2+ calls */


sbiret_t hal_sbiGetSpecVersion(void);


sbiret_t hal_sbiProbeExtension(long extid);


void hal_sbiSetTimer(u64 stime);


void hal_sbiReset(u32 type, u32 reason);


void _hal_sbiInit(void);


#endif
