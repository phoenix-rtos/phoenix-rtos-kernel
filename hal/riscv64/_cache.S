/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * riscv64 cache management
 *
 * Copyright 2024 Phoenix Systems
 * Author: Lukasz Leczkowski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#define __ASSEMBLY__

#include <board_config.h>

.text

/* void hal_cpuDCacheInval(void *va, size_t size)
 * va must have write access
 */
.global hal_cpuDCacheInval
.type hal_cpuDCacheInval, @function
hal_cpuDCacheInval:
#ifdef DCACHE_BLOCK_SIZE
	/* Align size to cache block size */
	li a4, DCACHE_BLOCK_SIZE
	addi a5, a4, -1
	add a1, a1, a5
	li a5, -DCACHE_BLOCK_SIZE
	and a1, a1, a5

	/* End address */
	add a5, a1, a0
	beq a0, a5, 2f

1:
	cbo.inval (a0)
	add a0, a0, a4
	bgtu a5, a0, 1b

2:
#endif
	ret
.size hal_cpuDCacheInval, . - hal_cpuDCacheInval


/* void hal_cpuDCacheFlush(void *va, size_t size)
 * va must have write access
 */
.global hal_cpuDCacheFlush
.type hal_cpuDCacheFlush, @function
hal_cpuDCacheFlush:
#ifdef DCACHE_BLOCK_SIZE
	/* Align size to cache block size */
	li a4, DCACHE_BLOCK_SIZE
	addi a5, a4, -1
	add a1, a1, a5
	li a5, -DCACHE_BLOCK_SIZE
	and a1, a1, a5

	/* End address */
	add a5, a1, a0
	beq a0, a5, 2f

1:
	cbo.flush (a0)
	add a0, a0, a4
	bgtu a5, a0, 1b

2:
#endif
	ret
.size hal_cpuDCacheFlush, . - hal_cpuDCacheFlush
