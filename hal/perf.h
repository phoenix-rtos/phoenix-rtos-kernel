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

#ifndef _HAL_PERF_H_
#define _HAL_PERF_H_


#include <hal/cpu.h>


extern int hal_perfStackUnwind(const cpu_context_t *ctx, const void *kstack, size_t kstacksz, ptr_t *cstack, size_t maxdepth);


#endif
