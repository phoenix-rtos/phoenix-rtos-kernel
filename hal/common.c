/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Common hal functions
 *
 * Copyright 2023 Phoenix Systems
 * Author: Hubert Badocha
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include "hal/cpu.h"
#include "hal/string.h"

#include "hal/types.h"

/* STACK_ALIGN is most strict stack alignment constraint across all supported architectures. */
#define STACK_ALIGN 16U


void hal_stackPutArgs(void **stackp, size_t argc, const struct stackArg *argv)
{
	size_t i, misalign, argsz = 0U;
	ptr_t stack = (ptr_t)*stackp;

	for (i = 0U; i < argc; i++) {
		argsz += SIZE_STACK_ARG(argv[i].sz);
	}

	misalign = (stack - argsz) & (STACK_ALIGN - 1U);
	stack -= misalign;

	for (i = 0U; i < argc; i++) {
		stack -= SIZE_STACK_ARG(argv[i].sz);
		hal_memcpy((void *)stack, argv[i].argp, argv[i].sz);
	}

	*stackp = (void *)stack;
}


#ifndef HAL_CPU_GETUSERCTX_CUSTOM
/* Default: userspace context is the saved one, or lies at the kstack top if the thread was in the kernel */
int hal_cpuGetUserCtx(cpu_context_t *ctx, const void *kstackTop, pmap_t *pmap, cpu_context_t *dst)
{
	const cpu_context_t *src = ctx;

	(void)pmap;

	if (hal_cpuSupervisorMode(ctx) != 0) {
		src = (const cpu_context_t *)((const u8 *)kstackTop - sizeof(cpu_context_t));
	}

	hal_memcpy(dst, src, sizeof(cpu_context_t));

	return 0;
}
#endif
