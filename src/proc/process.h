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

#define MAX_PID ((1LL << (__CHAR_BIT__ * (sizeof(unsigned)) - 1)) - 1)

typedef struct _process_t {
	struct _process_t *next;
	struct _process_t *prev;

	lock_t lock;

	struct _thread_t *threads;
	int refs;

	char *path;
	char **argv;
	char **envp;
	unsigned int id;
	rbnode_t idlinkage;

	union {
		vm_map_t map;
		map_entry_t *entries;
	};
	vm_map_t *mapp;
	int exit;
	
	unsigned lazy : 1;
	unsigned lgap : 1;
	unsigned rgap : 1;

	/*u32 uid;
	u32 euid;
	u32 suid;
	u32 gid;
	u32 egid;
	u32 sgid;
	u32 umask;*/

	void *ports;

	lock_t *rlock;
	rbtree_t *resources;
	rbtree_t resourcetree;

	unsigned sigpend;
	unsigned sigmask;
	void *sighandler;

	void *got;
} process_t;


extern process_t *proc_find(unsigned int pid);


extern int proc_put(process_t *proc);


extern void proc_get(process_t *proc);


extern void proc_kill(process_t *proc);


extern void proc_reap(void);


extern int proc_start(void (*initthr)(void *), void *arg, const char *path);


extern int proc_fileSpawn(const char *path, char **argv, char **envp);


extern int proc_syspageSpawn(syspage_program_t *program, const char *path, char **argv);


extern int proc_execve(const char *path, char **argv, char **envp);


extern int proc_vfork(void);


extern int proc_fork(void);


extern int proc_release(void);


extern void proc_exit(int code);


extern int _process_init(vm_map_t *kmap, vm_object_t *kernel);


extern void process_dumpException(unsigned int n, exc_context_t *exc);

#endif
