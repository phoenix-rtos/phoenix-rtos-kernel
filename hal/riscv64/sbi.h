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


/* Standard SBI Errors */
#define SBI_SUCCESS               0
#define SBI_ERR_FAILED            -1
#define SBI_ERR_NOT_SUPPORTED     -2
#define SBI_ERR_INVALID_PARAM     -3
#define SBI_ERR_DENIED            -4
#define SBI_ERR_INVALID_ADDRESS   -5
#define SBI_ERR_ALREADY_AVAILABLE -6
#define SBI_ERR_ALREADY_STARTED   -7
#define SBI_ERR_ALREADY_STOPPED   -8
#define SBI_ERR_NO_SHMEM          -9


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


__attribute__((noreturn)) void hal_sbiReset(u32 type, u32 reason);


sbiret_t hal_sbiSendIPI(unsigned long hart_mask, unsigned long hart_mask_base);


sbiret_t hal_sbiHartGetStatus(unsigned long hartid);


sbiret_t hal_sbiHartStart(unsigned long hartid, unsigned long start_addr, unsigned long opaque);


void hal_sbiRfenceI(unsigned long hart_mask, unsigned long hart_mask_base);


sbiret_t hal_sbiSfenceVma(unsigned long hart_mask, unsigned long hart_mask_base, unsigned long vaddr, unsigned long size);


sbiret_t hal_sbiSfenceVmaAsid(unsigned long hart_mask, unsigned long hart_mask_base, unsigned long vaddr, unsigned long size, unsigned long asid);


void _hal_sbiInit(void);


#endif
