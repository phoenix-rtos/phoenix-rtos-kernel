/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * SBI routines (RISCV64)
 *
 * Copyright 2018, 2020 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_SBI_H_
#define _HAL_SBI_H_

#include "../../include/errno.h"
#include "cpu.h"

#define SBI_SETTIMER                0
#define SBI_PUTCHAR                 1
#define SBI_GETCHAR                 2
#define SBI_CLEARIPI                3
#define SBI_SENDIPI                 4
#define SBI_REMOTE_FENCE_I          5
#define SBI_REMOTE_SFENCE_VMA       6
#define SBI_REMOTE_SFENCE_VMA_ASID  7
#define SBI_SHUTDOWN                8


typedef struct _sbiret_t {
	long error;
	long value;
} sbiret_t;


static inline sbiret_t sbi_ecall(int ext, int fid, u64 arg0, u64 arg1, u64 arg2, u64 arg3, u64 arg4, u64 arg5)
{
	sbiret_t ret;

	register u64 a0 asm ("a0") = arg0;
	register u64 a1 asm ("a1") = arg1;
	register u64 a2 asm ("a2") = arg2;
	register u64 a3 asm ("a3") = arg3;
	register u64 a4 asm ("a4") = arg4;
	register u64 a5 asm ("a5") = arg5;
	register u64 a6 asm ("a6") = fid;
	register u64 a7 asm ("a7") = ext;

	__asm__ volatile ("\
		ecall"
	: "+r" (a0), "+r" (a1)
	: "r" (a2), "r" (a3), "r" (a4), "r" (a5), "r" (a6), "r" (a7)
	: "memory");

	ret.error = a0;
	ret.value = a1;

	return ret;
}


#endif
