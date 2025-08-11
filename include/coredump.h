/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Coredump messages
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jakub Klimek
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_COREDUMP_H_
#define _PHOENIX_COREDUMP_H_

#include "types.h"

typedef struct {
	int tid;
	int nextTid;
	void *stackAddr;
	char context[];
} coredump_thread;

typedef struct {
	void *startAddr;
	void *endAddr;
} coredump_memseg;

typedef struct {
	void *pbase;
	void *vbase;
} coredump_reloc;

typedef struct {
	int pid;
	int tid;
	int signo;
	enum {
		COREDUMP_TYPE_32,
		COREDUMP_TYPE_64
	} type;
	char path[64];
	int memSegCnt;
	int threadCnt;
} coredump_general;

typedef struct {
	enum {
		COREDUMP_REQ_THREAD,
		COREDUMP_REQ_MEMLIST,
		COREDUMP_REQ_RELOC,
		COREDUMP_REQ_MEM,
	} type;
	union {
		struct {
			int tid;
		} thread;
		struct {
			void *startAddr;
			size_t size;
			__u32 responsePort;
		} mem;
	};
} coredump_req;


#endif
