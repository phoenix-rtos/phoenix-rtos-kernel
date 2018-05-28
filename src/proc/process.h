/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Processes management
 *
 * Copyright 2012-2015, 2016-2017 Phoenix Systems
 * Copyright 2001, 2006-2007 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Pawel Kolodziej, Jacek Popko
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#ifndef _PROC_PROCESS_H_
#define _PROC_PROCESS_H_

#include HAL
#include "../vm/vm.h"
#include "lock.h"
#include "../vm/amap.h"


typedef struct _process_t {
	struct _process_t *next;
	struct _process_t *prev;

	lock_t lock;

	struct _process_t *parent;
	struct _process_t *childs;

	struct _thread_t *threads;

	char *path;
	unsigned int id;
	rbnode_t idlinkage;

	struct _process_t *zombies;
	struct _thread_t *waitq;
	spinlock_t waitsl;
	int waitpid;

	enum { NORMAL = 0, ZOMBIE } state;

	/* Temporary? Used to cleanup after pmap */
	void *pmapv;
	page_t *pmapp;

	union {
		vm_map_t map;
		map_entry_t *entries;
	};
	vm_map_t *mapp;
	amap_t *amap;
	int exit;
	char lazy;

	/*u32 uid;
	u32 euid;
	u32 suid;
	u32 gid;
	u32 egid;
	u32 sgid;
	u32 umask;*/

	void *ports;
	rbtree_t resources;

	unsigned sigpend;
	unsigned sigmask;
	void *sighandler;

	void *got;
} process_t;


extern process_t *proc_find(unsigned int pid);


extern void proc_kill(process_t *proc);


extern int proc_start(void (*initthr)(void *), void *arg, const char *path);


extern int proc_copyexec(void);


extern int proc_execle(syspage_program_t *prog, const char *path, ...);


extern int proc_execve(syspage_program_t *prog, const char *path, char **argv, char **envp);


extern int proc_vfork(void);


extern void proc_exit(int code);


extern int _process_init(vm_map_t *kmap, vm_object_t *kernel);


#endif
