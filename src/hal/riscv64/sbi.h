/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * SBI routines (RISCV64)
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_SBI_H_
#define _HAL_SBI_H_

#include "../../../include/errno.h"
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


static inline u64 sbi_call(u64 n, u64 arg0, void *arg1, void *arg2)
{
	register u64 a0 asm ("a0") = (arg0);
	register void *a1 asm ("a1") = (arg1);
	register void *a2 asm ("a2") = (arg2);
	register void *a7 asm ("a7") = (void *)n;

	__asm__ volatile ("ecall" \
		: "+r" (a0) \
		: "r" (a1), "r" (a2), "r" (a7) \
		: "memory");

	return a0;
}


#endif
