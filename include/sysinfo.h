/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages
 *
 * Copyright 2018 Phoenix Systems
 * Author: Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_SYSINFO_H_
#define _PHOENIX_SYSINFO_H_

#ifndef HAL
#include <arch.h>
#endif


typedef struct _syspageprog_t {
	char name[16];
	addr_t addr;
	size_t size;
} syspageprog_t;


typedef struct _threadinfo_t {
	unsigned int pid;
	unsigned int tid;

	int load;
	unsigned int cpu_time;
	int priority;
	int state;
	int vmem;

	char name[32];
} threadinfo_t;


typedef struct _entryinfo_t {
	void *vaddr;
	size_t size;
	size_t anonsz;

	unsigned char flags;
	unsigned char prot;
	offs_t offs;

	enum { OBJECT_ANONYMOUS, OBJECT_MEMORY, OBJECT_OID } object;
	oid_t oid;
} entryinfo_t;


typedef struct _pageinfo_t {
	unsigned int count;
	addr_t addr;
	char marker;
} pageinfo_t;


typedef struct _meminfo_t {
	struct {
		unsigned int alloc, free, boot, sz;
		int mapsz;
		pageinfo_t *map;
	} page;

	struct {
		unsigned int pid, total, free, sz;
		int mapsz, kmapsz;
		entryinfo_t *kmap, *map;
	} entry;
} meminfo_t;


typedef struct {
		enum { perf_sche, perf_enqd, perf_wkup } type;
		time_t timestamp;
		time_t timeout;
		unsigned long tid;
		unsigned long pid;
} perf_event_t;

#endif
