/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Coredump syscall interface
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Klimek
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _PHOENIX_COREDUMP_H_
#define _PHOENIX_COREDUMP_H_


#include "types.h"


enum { coredumpInfoThread = 0,
	coredumpInfoMemList,
	coredumpInfoReloc };


typedef struct {
	int tid;
	int nextTid;
	void *stackAddr;
	char context[];
} coredump_thread_t;


typedef struct {
	void *startAddr;
	void *endAddr;
} coredump_memseg_t;


typedef struct {
	void *pbase;
	void *vbase;
} coredump_reloc_t;


typedef struct {
	int pid;
	int tid;
	unsigned int signo;
	enum { coredumpType32 = 0,
		coredumpType64 } type;
	char path[64];
	size_t memSegCnt;
	size_t threadCnt;
} coredump_general_t;


#endif
