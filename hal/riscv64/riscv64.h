/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * CPU related routines (RISCV64)
 *
 * Copyright 2018 Phoenix Systems
 * Author: Pawel Pisarczyk
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _HAL_RISCV64_H_
#define _HAL_RISCV64_H_


#include <arch/types.h>


#define csr_set(csr, val) \
	({ \
		unsigned long __v = (unsigned long)(val); \
		__asm__ volatile( \
			"csrs " #csr ", %0" \
			: \
			: "rK"(__v) \
			: "memory"); \
	})


#define csr_write(csr, val) \
	({ \
		unsigned long __v = (unsigned long)(val); \
		__asm__ volatile( \
			"csrw " #csr ", %0" \
			: \
			: "rK"(__v) \
			: "memory"); \
	})


#define csr_read(csr) \
	({ \
		register unsigned long __v; \
		__asm__ volatile( \
			"csrr %0, " #csr \
			: "=r"(__v) \
			: \
			: "memory"); \
		__v; \
	})


#define csr_clear(csr, val) \
	({ \
		unsigned long __v = (unsigned long)(val); \
		__asm__ volatile( \
			"csrc " #csr ", %0" \
			: \
			: "rK"(__v) \
			: "memory"); \
	})


static inline void hal_cpuFlushTLB(void *vaddr)
{
	(void)vaddr;

	__asm__ volatile("sfence.vma" ::);
}


static inline void hal_cpuSwitchSpace(addr_t pdir)
{
	/* clang-format off */
	__asm__ volatile(
		"sfence.vma\n\t"
		"csrw sptbr, %0\n\t"
		::"r"(pdir));
	/* clang-format on */
}


#endif
