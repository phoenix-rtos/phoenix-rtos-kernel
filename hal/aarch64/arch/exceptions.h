/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2017, 2018, 2024 Phoenix Systems
 * Author: Pawel Pisarczyk, Jakub Sejdak, Aleksander Kaminski, Jacek Maksymowicz
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_AARCH64_EXCEPTIONS_H_
#define _PH_HAL_AARCH64_EXCEPTIONS_H_

#include "cpu.h"

#define EXC_DEFAULT   64
#define EXC_PAGEFAULT 65

#define EXC_UNDEFINED               0x00
#define EXC_TRAP_WFI_WFE            0x01
#define EXC_TRAP_MCR_MRC_CP15       0x03
#define EXC_TRAP_MCRR_MRRC_CP15     0x04
#define EXC_TRAP_MCR_MRC_CP14       0x05
#define EXC_TRAP_LDC_STC            0x06
#define EXC_TRAP_MRRC_CP14          0x0c
#define EXC_ILLEGAL_EXEC_STATE      0x0e
#define EXC_SVC_AA32                0x11
#define EXC_TRAP_MSRR_MRRS_SYS_AA64 0x14
#define EXC_TRAP_MSR_MRS_SYS_AA64   0x18
#define EXC_INSTR_ABORT_EL0         0x20
#define EXC_INSTR_ABORT_EL1         0x21
#define EXC_PC_ALIGN                0x22
#define EXC_DATA_ABORT_EL0          0x24
#define EXC_DATA_ABORT_EL1          0x25
#define EXC_SP_ALIGN                0x26
#define EXC_TRAP_FPU_AA32           0x28
#define EXC_TRAP_FPU_AA64           0x2c
#define EXC_SERROR                  0x2f
#define EXC_BREAKPOINT_EL0          0x30
#define EXC_BREAKPOINT_EL1          0x31
#define EXC_STEP_EL0                0x32
#define EXC_STEP_EL1                0x33
#define EXC_WATCHPOINT_EL0          0x34
#define EXC_WATCHPOINT_EL1          0x35
#define EXC_BKPT_AA32               0x38
#define EXC_BRK_AA64                0x3c

#define SIZE_CTXDUMP 1024 /* Size of dumped context string */


typedef struct _exc_context_t {
	u64 esr;
	u64 far;
	cpu_context_t cpuCtx;
} exc_context_t;

#endif
