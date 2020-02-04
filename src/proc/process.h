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
#include "file.h"
#include "../../include/signal.h"

#define MAX_PID ((1LL << (__CHAR_BIT__ * (sizeof(unsigned)) - 1)) - 1)

#define PFL_STOPPED 1
#define PFL_CONTINUED 2
#define PFL_ZOMBIE 4
#define PFL_NOREAP 8

typedef struct _process_t {
	lock_t lock;

	struct _thread_t *threads;
	struct _thread_t *ghosts;
	struct _thread_t *reaper;
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

	unsigned lazy : 1;
	unsigned lgap : 1;
	unsigned rgap : 1;

	void *ports;

	rbtree_t resources;

	unsigned sigpend;
	unsigned sigmask;
	void *sigtrampoline;
	sighandler_t *sighandlers;

	unsigned char signum;
	unsigned exit;
	unsigned flags;

	struct _thread_t *wait;
	struct _process_t *children;
	struct _process_t *next, *prev;

	struct _process_group_t *group;
	struct _process_t *pg_next, *pg_prev;

	pid_t ppid;

	struct _iodes_t *cwd;
	unsigned hcount;
	struct _hades_t *handles;

	void *got;
} process_t;


typedef struct _process_group_t {
	pid_t id;
	process_t *members;

	struct _session_t *session;
	struct _process_group_t *next, *prev;
} process_group_t;


typedef struct _session_t {
	pid_t id;
	struct _iodes_t *ctty;
	process_group_t *members;
} session_t;


#define process_lock(p) proc_lockSet(&p->lock)
#define process_unlock(p) proc_lockClear(&p->lock)


extern process_t *proc_find(unsigned int pid);


extern int proc_put(process_t *proc);


extern void proc_get(process_t *proc);


extern void proctree_lock(void);


extern void proctree_unlock(void);


extern void proc_kill(process_t *proc);


extern void proc_reap(void);


extern int proc_sigaction(int sig, const struct sigaction *act, struct sigaction *oact);


extern int proc_start(void (*initthr)(void *), void *arg, const char *path);


extern int proc_fileSpawn(const char *path, char **argv, char **envp);


extern int proc_syspageSpawn(syspage_program_t *program, const char *path, char **argv);


extern int proc_exec(int dirfd, const char *path, char **argv, char **envp);


extern int proc_sigpost(int pid, int sig);


extern int proc_vfork(void);


extern int proc_fork(void);


extern int proc_release(void);


extern void proc_exit(int code);


extern int _process_init(vm_map_t *kmap, vm_object_t *kernel);


extern void process_dumpException(unsigned int n, exc_context_t *exc);

#endif
