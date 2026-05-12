/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Exception handling
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PH_HAL_RISCV64_EXCEPTIONS_H_
#define _PH_HAL_RISCV64_EXCEPTIONS_H_

#include "cpu.h"


#define EXC_DEFAULT   128U
#define EXC_UNDEFINED 2U
#define EXC_PAGEFAULT 127U

#define SIZE_CTXDUMP 1024U /* Size of dumped context */

typedef cpu_context_t exc_context_t;


typedef struct {
	u64 ra;
	u64 sp;
	u64 s0;
	u64 s1;
	u64 s2;
	u64 s3;
	u64 s4;
	u64 s5;
	u64 s6;
	u64 s7;
	u64 s8;
	u64 s9;
	u64 s10;
	u64 s11;
	u64 sstatus;
} __attribute__((packed, aligned(8))) excjmp_context_t;

#endif
