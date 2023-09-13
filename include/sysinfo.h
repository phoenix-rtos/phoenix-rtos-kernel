/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages
 *
 * Copyright 2018, 2021 Phoenix Systems
 * Author: Jan Sikorski, Lukasz Kosinski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PHOENIX_SYSINFO_H_
#define _PHOENIX_SYSINFO_H_


typedef struct {
	char name[32];
	addr_t addr;
	size_t size;
} syspageprog_t;


typedef struct {
	unsigned int pid;
	unsigned int tid;
	unsigned int ppid;

	int load;
	time_t cpuTime;
	int priority;
	int state;
	int vmem;
	time_t wait;

	char name[128];
} threadinfo_t;


typedef struct {
	void *vaddr;
	size_t size;
	size_t anonsz;

	unsigned char flags;
	unsigned char prot;
	off_t offs;

	enum { OBJECT_ANONYMOUS, OBJECT_MEMORY, OBJECT_OID } object;
	oid_t oid;
} entryinfo_t;


typedef struct {
	unsigned int count;
	addr_t addr;
	char marker;
} pageinfo_t;


typedef struct {
	int id;
	addr_t pstart;
	addr_t pend;
	addr_t vstart;
	addr_t vend;
	size_t alloc;
	size_t free;
} mapinfo_t;


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

	struct {
		size_t total;
		size_t free;
		int mapsz;
		mapinfo_t *map;
	} maps;
} meminfo_t;


enum { perf_evScheduling, perf_evEnqueued, perf_evWaking, perf_evPreempted };


typedef struct {
	unsigned int deltaTimestamp : 12;
	unsigned int type : 2;
	unsigned int tid : 18;
} __attribute__((packed)) perf_event_t;


enum { perf_levBegin, perf_levEnd, perf_levFork, perf_levKill, perf_levExec };


typedef struct {
	unsigned int sbz;

	unsigned int deltaTimestamp : 12;
	unsigned int type : 3;

	unsigned int prio : 3;
	unsigned int tid : 18;
	unsigned int pid : 18;
} __attribute__((packed)) perf_levent_begin_t;


typedef struct {
	unsigned int sbz;

	unsigned int deltaTimestamp : 12;
	unsigned int type : 3;

	unsigned int tid : 18;
} __attribute__((packed)) perf_levent_end_t;


typedef struct {
	unsigned int sbz;

	unsigned int deltaTimestamp : 12;
	unsigned int type : 3;

	unsigned int tid: 18;
	unsigned int ppid : 18;
	unsigned int pid : 18;
} __attribute__((packed)) perf_levent_fork_t;


typedef struct {
	unsigned int sbz;

	unsigned int deltaTimestamp : 12;
	unsigned int type : 3;

	unsigned int tid: 18;
	unsigned int pid : 18;
} __attribute__((packed)) perf_levent_kill_t;


typedef struct {
	unsigned int sbz;

	unsigned int deltaTimestamp : 12;
	unsigned int type : 3;

	unsigned int tid: 18;
	char path[32];
} __attribute__((packed)) perf_levent_exec_t;


#endif
