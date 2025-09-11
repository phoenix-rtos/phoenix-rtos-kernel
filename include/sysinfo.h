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

#ifndef _PH_SYSINFO_H_
#define _PH_SYSINFO_H_


typedef struct _syspageprog_t {
	char name[32];
	addr_t addr;
	size_t size;
} syspageprog_t;


typedef struct _threadinfo_t {
	pid_t pid;
	unsigned int tid;
	pid_t ppid;

	int load;
	time_t cpuTime;
	int priority;
	int state;
	int vmem;
	time_t wait;

	char name[128];
} __attribute__((packed)) threadinfo_t;


typedef struct _entryinfo_t {
	void *vaddr;
	size_t size;
	size_t anonsz;

	unsigned char flags;
	unsigned char prot;
	unsigned char protOrig;
	unsigned long long offs;

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


#endif
