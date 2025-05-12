/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Machine-dependent profiling subsystem routines
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */

#include <hal/perf.h>


int hal_perfStackUnwind(const cpu_context_t *ctx, const void *kstack, size_t kstacksz, ptr_t *cstack, size_t maxdepth)
{
	ptr_t *fp, *new_fp;
	u32 d = 0;

	fp = (ptr_t *)ctx->ebp;

	cstack[d] = ctx->eip;
	d++;

	while ((ptr_t)kstack <= (ptr_t)fp && (ptr_t)fp < (ptr_t)kstack + kstacksz) {
		if (d >= maxdepth) {
			break;
		}

		cstack[d] = fp[1];
		d++;

		new_fp = (ptr_t *)fp[0];

		if (new_fp <= fp) {
			break;
		}

		fp = new_fp;
	}

	return d;
}
