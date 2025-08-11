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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */


#include "hal/cpu.h"
#include "hal/string.h"

#include "hal/types.h"

/* STACK_ALIGN is most strict stack alignment constraint across all supported architectures. */
#define STACK_ALIGN 16U


void hal_stackPutArgs(void **stackp, size_t argc, const struct stackArg *argv)
{
	size_t i, misalign, argsz = 0;
	ptr_t stack = (ptr_t)*stackp;

	for (i = 0; i < argc; i++) {
		argsz += SIZE_STACK_ARG(argv[i].sz);
	}

	misalign = (stack - argsz) & (STACK_ALIGN - 1U);
	stack -= misalign;

	for (i = 0; i < argc; i++) {
		stack -= SIZE_STACK_ARG(argv[i].sz);
		/* MISRA Rule 11.6: Casted stack to a pointer type */
		hal_memcpy((void *)(unsigned int *)stack, argv[i].argp, argv[i].sz);
	}

	/* MISRA Rule 11.6: Casted stack to a pointer type */
	*stackp = (void *)(unsigned int *)stack;
}
