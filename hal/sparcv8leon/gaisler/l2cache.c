/*
 * Phoenix-RTOS
 *
 * Operating system loader
 *
 * L2 cache management
 *
 * Copyright 2025 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "l2cache.h"

#include "hal/sparcv8leon/sparcv8leon.h"

#include <arch/pmap.h>
#include <arch/cpu.h>

/* clang-format off */
enum { l2c_ctrl = 0, l2c_status, l2c_fma, l2c_fsi, l2c_err = 8, l2c_erra, l2c_tcb, l2c_dcb, l2c_scrub,
	l2c_sdel, l2c_einj, l2c_accc };
/* clang-format on */


static struct {
	volatile u32 *base;
	size_t lineSz; /* bytes */
} l2c_common;


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
void l2c_flushRange(unsigned int mode, ptr_t start, size_t size)
{
	u32 val;
	ptr_t fstart;
	ptr_t fend;
	volatile u32 *freg = l2c_common.base + l2c_fma;
	mode &= 0x7U;

	/* TN-0021: flush register accesses must be done using atomic operations */

	if ((mode >= (unsigned int)l2c_inv_all) && (mode <= (unsigned int)l2c_flush_inv_all)) {
		/* clang-format off */
		__asm__ volatile ("swap [%1], %0" : "+r"(mode) : "r"(freg) : "memory");
		/* clang-format on */
	}
	else {
		fstart = start & ~(l2c_common.lineSz - 1U);
		fend = fstart + ((((start & (l2c_common.lineSz - 1U)) + size) + (l2c_common.lineSz - 1U)) & ~(l2c_common.lineSz - 1U));

		while (fstart < fend) {
			val = fstart | mode;

			/* Flushing takes 5 cycles/line */

			/* clang-format off */
			__asm__ volatile (
				"swap [%1], %0\n\t"
				 : "+r"(val)
				 : "r"(freg)
				 : "memory"
			);
			/* clang-format on */

			fstart += l2c_common.lineSz;
		}
	}
}


/* parasoft-suppress-next-line MISRAC2012-DIR_4_3 "Assembly is required for low-level operations" */
void l2c_init(addr_t base)
{
	u32 reg;
	size_t lineSz;

	l2c_common.base = _pmap_halMapDevice(PAGE_ALIGN(base), PAGE_OFFS(base), SIZE_PAGE);

	reg = *(l2c_common.base + l2c_status);
	lineSz = ((reg & (1UL << 24)) != 0UL) ? 64U : 32U;

	l2c_common.lineSz = lineSz;

	l2c_flushRange(l2c_inv_all, 0, 0);

	/* Wait for flush to complete:
	 * Full L2 cache invalidation takes 5 cycles for the 1st line
	 * and 1 cycle for each subsequent line.
	 * There are 0x8000 lines.
	 */

	/* clang-format off */
	__asm__ volatile (
		"set 0x2001, %%g1\n\t"
	"1:\n\t"
		"nop\n\t"
		"subcc %%g1, 1, %%g1\n\t"
		"bne 1b\n\t"
		"nop\n\t"
		::: "g1", "cc"
	);
	/* clang-format on */

	/* Initialize cache according to GRLIB-TN-0021 errata */
	*(l2c_common.base + l2c_err) = 0;
	*(l2c_common.base + l2c_accc) = ((1UL << 14) | (1UL << 13) | (1UL << 10) | (1UL << 4) | (1UL << 2) | (1UL << 1));

	/* Enable cache with default params, EDAC disabled, LRU */
	*(l2c_common.base + l2c_ctrl) = (1UL << 31);

	hal_cpuDataStoreBarrier();

	/* Perform load from cacheable memory (errata) */
	/* clang-format off */
	__asm__ volatile ("ld [%0], %%g0" :: "r"(VADDR_KERNEL): "memory");
	/* clang-format on */
}
