/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Performance analysis subsystem
 *
 * Copyright 2025 Phoenix Systems
 * Author: Adam Greloch
 *
 * %LICENSE%
 */


#ifndef _PHOENIX_PERF_H_
#define _PHOENIX_PERF_H_


/* clang-format off */
typedef enum { perf_mode_threads, perf_mode_trace, perf_mode_count } perf_mode_t;
typedef enum { trace_channel_meta, trace_channel_event, trace_channel_count } trace_channel_t;
/* clang-format on */


#define PERF_TRACE_FLAG_ROLLING (1 << 0) /* treat event channel as rolling window */


#endif
