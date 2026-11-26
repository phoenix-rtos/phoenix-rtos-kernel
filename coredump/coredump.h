/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Coredump support
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Klimek
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#ifndef _COREDUMP_H_
#define _COREDUMP_H_

#include "proc/process.h"
#include "include/coredump.h"


#ifdef COREDUMP_SUPPORT


/* Hand over a crashed process holding its last reference. */
extern int coredump_enqueue(process_t *process);


/* Check if dying process wasn't the active coredump server */
extern void coredump_processDestroyed(process_t *process);


/* Wait for crashed process to dump and claim it */
extern int coredump_wait(coredump_general_t *info);


extern int coredump_info(int type, int tid, void *buf, size_t size);


extern int coredump_map(const void *addr, size_t len, const void **ptr);


/* Release dumped process */
extern int coredump_done(void);


extern void _coredump_init(void);


#endif /* COREDUMP_SUPPORT */


#endif /* _COREDUMP_H_ */
