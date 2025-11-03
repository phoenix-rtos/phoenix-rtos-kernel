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

#include "hal/hal.h"
#include "vm/vm.h"
#include "lock.h"
#include "vm/amap.h"
#include "syspage.h"
#include "lib/lib.h"

#define MAX_PID MAX_ID

typedef struct _process_t {
	lock_t lock;

	struct _thread_t *threads;
	struct _thread_t *ghosts;
	struct _thread_t *reaper;
	int refs;

	char *path;
	char **argv;
	char **envp;
	idnode_t idlinkage;

	vm_map_t map;
	map_entry_t *entries;
	vm_map_t *mapp;
	vm_map_t *imapp;
	pmap_t *pmapp;
	int exit;

	unsigned int lazy : 1;
	unsigned int lgap : 1;
	unsigned int rgap : 1;

#if 0 /* TODO: Process shall keep information permissions */
	u32 uid;
	u32 euid;
	u32 suid;
	u32 gid;
	u32 egid;
	u32 sgid;
	u32 umask;
#endif

	struct _port_t *ports;

	idtree_t resources;

	unsigned int sigpend;
	unsigned int sigmask;
	void (*sighandler)(void);

	void *got;
	hal_tls_t tls;
} process_t;


static inline int process_getPid(const process_t *process)
{
	return process->idlinkage.id;
}


extern process_t *proc_find(int pid);


extern int proc_put(process_t *p);


extern void proc_get(process_t *p);


extern void proc_kill(process_t *proc);


extern void proc_reap(void);


extern int proc_start(void (*initthr)(void *harg), void *arg, const char *path);


extern int proc_fileSpawn(const char *path, char **argv, char **envp);


extern int proc_syspageSpawnName(const char *imap, const char *dmap, const char *name, char **argv);


extern int proc_syspageSpawn(const syspage_prog_t *program, vm_map_t *imap, vm_map_t *map, const char *path, char **argv);


extern int proc_execve(const char *path, char **argv, char **envp);


extern int proc_sigpost(int pid, int sig);


extern int proc_vfork(void);


extern int proc_fork(void);


extern int proc_release(void);


extern void proc_exit(int code);


extern int _process_init(vm_map_t *kmap, vm_object_t *kernel);


extern void process_dumpException(unsigned int n, exc_context_t *ctx);


extern int process_tlsInit(hal_tls_t *dest, hal_tls_t *source, vm_map_t *map);


extern int process_tlsDestroy(hal_tls_t *tls, vm_map_t *map);


#endif
