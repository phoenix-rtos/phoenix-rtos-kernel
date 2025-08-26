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


typedef struct _syspageprog_t {
	char name[32];
	addr_t addr;
	size_t size;
} syspageprog_t;


typedef struct _threadinfo_t {
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


typedef struct _entryinfo_t {
	void *vaddr;
	size_t size;
	size_t anonsz;

	unsigned char flags;
	unsigned char prot;
	unsigned char protOrig;
	off_t offs;

	enum { OBJECT_ANONYMOUS,
		OBJECT_MEMORY,
		OBJECT_OID } object;
	oid_t oid;
} entryinfo_t;


typedef struct _pageinfo_t {
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
	char name[16];
} mapinfo_t;


/* TODO: Consider changing type of kmaps mapsz from int to unsigned int */
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


enum { perf_evScheduling,
	perf_evEnqueued,
	perf_evWaking,
	perf_evPreempted };


typedef struct {
	unsigned deltaTimestamp : 12;
	unsigned type : 2;
	unsigned tid : 18;
} __attribute__((packed)) perf_event_t;


enum { perf_levBegin,
	perf_levEnd,
	perf_levFork,
	perf_levKill,
	perf_levExec };


typedef struct {
	unsigned sbz;

	unsigned deltaTimestamp : 12;
	unsigned type : 3;

	unsigned prio : 3;
	unsigned tid : 18;
	unsigned pid : 18;
} __attribute__((packed)) perf_levent_begin_t;


typedef struct {
	unsigned sbz;

	unsigned deltaTimestamp : 12;
	unsigned type : 3;

	unsigned tid : 18;
} __attribute__((packed)) perf_levent_end_t;


typedef struct {
	unsigned sbz;

	unsigned deltaTimestamp : 12;
	unsigned type : 3;

	unsigned tid : 18;
	unsigned ppid : 18;
	unsigned pid : 18;
} __attribute__((packed)) perf_levent_fork_t;


typedef struct {
	unsigned sbz;

	unsigned deltaTimestamp : 12;
	unsigned type : 3;

	unsigned tid : 18;
	unsigned pid : 18;
} __attribute__((packed)) perf_levent_kill_t;


typedef struct {
	unsigned sbz;

	unsigned deltaTimestamp : 12;
	unsigned type : 3;

	unsigned tid : 18;
	char path[32];
} __attribute__((packed)) perf_levent_exec_t;

#endif
