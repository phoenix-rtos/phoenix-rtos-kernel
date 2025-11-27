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

#define EXC_DEFAULT   64U
#define EXC_PAGEFAULT 65U

#define EXC_UNDEFINED               0x00U
#define EXC_TRAP_WFI_WFE            0x01U
#define EXC_TRAP_MCR_MRC_CP15       0x03U
#define EXC_TRAP_MCRR_MRRC_CP15     0x04U
#define EXC_TRAP_MCR_MRC_CP14       0x05U
#define EXC_TRAP_LDC_STC            0x06U
#define EXC_TRAP_MRRC_CP14          0x0cU
#define EXC_ILLEGAL_EXEC_STATE      0x0eU
#define EXC_SVC_AA32                0x11U
#define EXC_TRAP_MSRR_MRRS_SYS_AA64 0x14U
#define EXC_TRAP_MSR_MRS_SYS_AA64   0x18U
#define EXC_INSTR_ABORT_EL0         0x20U
#define EXC_INSTR_ABORT_EL1         0x21U
#define EXC_PC_ALIGN                0x22U
#define EXC_DATA_ABORT_EL0          0x24U
#define EXC_DATA_ABORT_EL1          0x25U
#define EXC_SP_ALIGN                0x26U
#define EXC_TRAP_FPU_AA32           0x28U
#define EXC_TRAP_FPU_AA64           0x2cU
#define EXC_SERROR                  0x2fU
#define EXC_BREAKPOINT_EL0          0x30U
#define EXC_BREAKPOINT_EL1          0x31U
#define EXC_STEP_EL0                0x32U
#define EXC_STEP_EL1                0x33U
#define EXC_WATCHPOINT_EL0          0x34U
#define EXC_WATCHPOINT_EL1          0x35U
#define EXC_BKPT_AA32               0x38U
#define EXC_BRK_AA64                0x3cU

#define SIZE_CTXDUMP 1024U /* Size of dumped context string */


typedef struct _exc_context_t {
	u64 esr;
	u64 far;
	cpu_context_t cpuCtx;
} exc_context_t;

#endif
