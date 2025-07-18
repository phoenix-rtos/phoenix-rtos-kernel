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

#ifndef _PERF_PERF_H_
#define _PERF_PERF_H_

#include "include/perf.h"
#include "vm/map.h"


extern int _perf_init(vm_map_t *kmap);


extern int perf_start(perf_mode_t mode, unsigned pid);


extern int perf_read(perf_mode_t mode, void *buf, size_t bufsz);


extern int perf_stop(perf_mode_t mode);


extern int perf_finish(perf_mode_t mode);


#endif
