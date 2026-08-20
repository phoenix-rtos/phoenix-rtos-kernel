/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * System calls
 *
 * Copyright 2012, 2017 Phoenix Systems
 * Copyright 2007 Pawel Pisarczyk
 * Author: Pawel Pisarczyk, Aleksander Kaminski, Jan Sikorski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* parasoft-begin-suppress MISRAC2012-RULE_8_4-a "Compatible function declaration is not possible for syscalls" */

#include "hal/hal.h"
#include "hal/cpu.h"
#include "include/errno.h"
#include "include/sysinfo.h"
#include "include/mman.h"
#include "include/sched.h"
#include "include/syscalls.h"
#include "include/threads.h"
#include "include/utsname.h"
#include "include/time.h"
#include "include/perf.h"
#include "lib/lib.h"
#include "lib/usermem.h"
#include "proc/proc.h"
#include "vm/object.h"
#include "posix/posix.h"
#include "syspage.h"
#include "perf/perf.h"
#include "perf/trace-events.h"

#define SYSCALLS_NAME(name) syscalls_##name,

/*
 * Kernel
 */


void syscalls_debug(u8 *ustack)
{
	const char *s;
	char *ks;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, const char *, s, 0U);
			},
			{
				return;
			});

	/* Ensure the string starts in process memory, as strndup also accepts strings fully-contained in kernel */
	if (vm_mapBelongs(proc_current()->process, s, 1U, PROT_READ) < 0) {
		return;
	}

	/* Copy the string as some implementations of hal_consolePrint take a spinlock */
	if (usermem_strndup(s, STR_MAX, &ks) < 0) {
		return;
	}

	hal_consolePrint(ATTR_USER, ks);

	vm_kfree(ks);
}


/*
 * Memory management
 */


int syscalls_sys_mmap(u8 *ustack)
{
	void **vaddr;
	void *kvaddr;
	size_t size;
	int prot, fildes, sflags;
	vm_flags_t flags;
	off_t offs;
	vm_object_t *o;
	oid_t oid;
	process_t *proc = proc_current()->process;
	int err;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, void **, vaddr, 0U);
				GETFROMSTACK(ustack, size_t, size, 1U);
				GETFROMSTACK(ustack, int, prot, 2U);
				GETFROMSTACK(ustack, int, sflags, 3U);
				GETFROMSTACK(ustack, int, fildes, 4U);
				GETFROMSTACK(ustack, off_t, offs, 5U);
			},
			{
				return -EFAULT;
			});

	flags = (vm_flags_t)sflags;
	size = round_page(size);

	if (vm_mapBelongs(proc, vaddr, sizeof(*vaddr), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	if ((flags & (MAP_DEVICE | MAP_PHYSMEM)) == MAP_DEVICE) {
		/* MAP_DEVICE without MAP_PHYSMEM can lead to undefined behavior within vm subsystem */
		return -EINVAL;
	}

	USERMEM_TRY(
			{
				kvaddr = *vaddr;
			},
			{
				return -EFAULT;
			});

	if ((flags & MAP_ANONYMOUS) != 0U) {
		if ((flags & MAP_PHYSMEM) != 0U) {
			o = VM_OBJ_PHYSMEM;
		}
		else if ((flags & MAP_CONTIGUOUS) != 0U) {
			o = vm_objectContiguous(size);
			if (o == NULL) {
				return -ENOMEM;
			}
		}
		else {
			o = NULL;
		}
	}
	else {
		err = posix_getOid(fildes, &oid);
		if (err < 0) {
			return err;
		}
		err = vm_objectGet(&o, oid);
		if (err < 0) {
			return err;
		}
	}

	flags &= ~(MAP_ANONYMOUS | MAP_CONTIGUOUS | MAP_PHYSMEM | MAP_NEEDSCOPY);

	kvaddr = vm_mmap(proc_current()->process->mapp, kvaddr, NULL, size, PROT_USER | (vm_prot_t)prot, o, (o == NULL) ? -1 : offs, flags);
	(void)vm_objectPut(o);

	if (kvaddr == NULL) {
		/* TODO: pass specific errno from vm_mmap */
		return -ENOMEM;
	}

	USERMEM_TRY(
			{
				*vaddr = kvaddr;
			},
			{
				(void)vm_munmap(proc_current()->process->mapp, kvaddr, size);
				return -EFAULT;
			});

	return EOK;
}


int syscalls_sys_munmap(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	void *vaddr;
	size_t size;
	int err;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, void *, vaddr, 0U);
				GETFROMSTACK(ustack, size_t, size, 1U);
			},
			{
				return -EFAULT;
			});

	size = round_page(size);
	err = vm_munmap(proc->mapp, vaddr, size);
	if (err < 0) {
		return err;
	}
	return EOK;
}


int syscalls_sys_mprotect(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	void *vaddr;
	size_t len;
	int prot, err;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, void *, vaddr, 0U);
				GETFROMSTACK(ustack, size_t, len, 1U);
				GETFROMSTACK(ustack, int, prot, 2U);
			},
			{
				return -EFAULT;
			});

	err = vm_mprotect(proc->mapp, vaddr, len, PROT_USER | (vm_prot_t)prot);
	if (err < 0) {
		return err;
	}
	return EOK;
}


/*
 * Process management
 */


int syscalls_vforksvc(u8 *ustack)
{
	return proc_vfork();
}


int syscalls_sys_fork(u8 *ustack)
{
	return proc_fork();
}


int syscalls_release(u8 *ustack)
{
	return proc_release();
}


int syscalls_sys_spawn(u8 *ustack)
{
	char *path;
	char **argv;
	char **envp;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, char *, path, 0U);
				GETFROMSTACK(ustack, char **, argv, 1U);
				GETFROMSTACK(ustack, char **, envp, 2U);
			},
			{
				return -EFAULT;
			});

	/* Ensure strings starts in process memory, as usermem also accepts strings fully-contained in kernel */
	if (vm_mapBelongs(proc_current()->process, path, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}
	if (argv != NULL && vm_mapBelongs(proc_current()->process, argv, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}
	if (envp != NULL && vm_mapBelongs(proc_current()->process, envp, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}

	return proc_fileSpawn(path, argv, envp);
}


int syscalls_exec(u8 *ustack)
{
	char *path;
	char **argv;
	char **envp;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, char *, path, 0U);
				GETFROMSTACK(ustack, char **, argv, 1U);
				GETFROMSTACK(ustack, char **, envp, 2U);
			},
			{
				return -EFAULT;
			});

	/* Ensure strings starts in process memory, as usermem also accepts strings fully-contained in kernel */
	if (vm_mapBelongs(proc_current()->process, path, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}
	if (argv != NULL && vm_mapBelongs(proc_current()->process, argv, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}
	if (envp != NULL && vm_mapBelongs(proc_current()->process, envp, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}

	return proc_execve(path, argv, envp);
}


int syscalls_spawnSyspage(u8 *ustack)
{
	char *imap;
	char *dmap;
	char *name;
	char **argv;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, char *, imap, 0U);
				GETFROMSTACK(ustack, char *, dmap, 1U);
				GETFROMSTACK(ustack, char *, name, 2U);
				GETFROMSTACK(ustack, char **, argv, 3U);
			},
			{
				return -EFAULT;
			});

	/* Ensure strings starts in process memory, as usermem also accepts strings fully-contained in kernel */
	if (imap != NULL && vm_mapBelongs(proc_current()->process, imap, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}
	if (dmap != NULL && vm_mapBelongs(proc_current()->process, dmap, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}
	if (vm_mapBelongs(proc_current()->process, name, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}
	if (argv != NULL && vm_mapBelongs(proc_current()->process, argv, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}

	return proc_syspageSpawnName(imap, dmap, name, argv);
}


int syscalls_sys_exit(u8 *ustack)
{
	int code;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, code, 0U);
			},
			{
				return -EFAULT;
			});

	proc_exit(code);
	return EOK;
}


int syscalls_sys_waitpid(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int pid, *status, options;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, pid, 0U);
				GETFROMSTACK(ustack, int *, status, 1U);
				GETFROMSTACK(ustack, int, options, 2U);
			},
			{
				return -EFAULT;
			});

	if ((status != NULL) && (vm_mapBelongs(proc, status, sizeof(*status), PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	return posix_waitpid(pid, status, (unsigned int)options);
}


int syscalls_threadJoin(u8 *ustack)
{
	int tid;
	time_t timeout;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, tid, 0U);
				GETFROMSTACK(ustack, time_t, timeout, 1U);
			},
			{
				return -EFAULT;
			});

	return proc_join(tid, timeout);
}


int syscalls_getpid(u8 *ustack)
{
	return process_getPid(proc_current()->process);
}


int syscalls_getppid(u8 *ustack)
{
	return posix_getppid(process_getPid(proc_current()->process));
}


/*
 * Thread management
 */


int syscalls_gettid(u8 *ustack)
{
	return proc_getTid(proc_current());
}


int syscalls_beginthreadex(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	startFn_t start;
	unsigned int priority, stacksz; /* FIXME: stacksz should probably be size_t */
	void *stack, *arg;
	int *id;
	int err;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, startFn_t, start, 0U);
				GETFROMSTACK(ustack, unsigned int, priority, 1U);
				GETFROMSTACK(ustack, void *, stack, 2U);
				GETFROMSTACK(ustack, unsigned int, stacksz, 3U);
				GETFROMSTACK(ustack, void *, arg, 4U);
				GETFROMSTACK(ustack, int *, id, 5U);
			},
			{
				return -EFAULT;
			});

	if ((id != NULL) && (vm_mapBelongs(proc, id, sizeof(*id), PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	if ((stack != NULL) && (vm_mapBelongs(proc, stack, stacksz, PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	if (priority > (u8)-1) {
		return -EINVAL;
	}

	proc_get(proc);

	err = proc_threadCreate(proc, start, id, (u8)priority, (size_t)SIZE_KSTACK, stack, (size_t)stacksz, proc_current()->sigmask, arg);

	if (err < 0) {
		(void)proc_put(proc);
	}

	return err;
}


__attribute__((noreturn)) void syscalls_endthread(u8 *ustack)
{
	proc_threadEnd();
}


int syscalls_nsleep(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	time_t *sec;
	long int *nsec;
	time_t ksec;
	long int knsec;
	int clockid;
	int flags;
	int ret;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, time_t *, sec, 0U);
				GETFROMSTACK(ustack, long int *, nsec, 1U);
				GETFROMSTACK(ustack, int, clockid, 2U);
				GETFROMSTACK(ustack, int, flags, 3U);
			},
			{
				return -EFAULT;
			});

	/* Not used right now, future-proofing */
	(void)clockid;

	if (vm_mapBelongs(proc, sec, sizeof(*sec), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	if (vm_mapBelongs(proc, nsec, sizeof(*nsec), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	USERMEM_TRY(
			{
				ksec = *sec;
				knsec = *nsec;
			},
			{
				return -EFAULT;
			});

	ret = proc_threadNanoSleep(&ksec, &knsec, (((unsigned int)flags & TIMER_ABSTIME) != 0U) ? 1 : 0);

	USERMEM_TRY(
			{
				*sec = ksec;
				*nsec = knsec;
			},
			{
				return -EFAULT;
			});

	return ret;
}


int syscalls_priority(u8 *ustack)
{
	int priority;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, priority, 0U);
			},
			{
				return -EFAULT;
			});

	return proc_threadPriority(priority);
}


int syscalls_schedInfo(u8 *ustack)
{
	int err, policy;
	process_t *proc;
	pid_t pid;
	sched_info_t *info;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, pid_t, pid, 0U);
				GETFROMSTACK(ustack, int, policy, 1U);
				GETFROMSTACK(ustack, sched_info_t *, info, 2U);
			},
			{
				return -EFAULT;
			});

	proc = proc_find(pid);
	if (proc == NULL) {
		return -EINVAL;
	}

	if (vm_mapBelongs(proc_current()->process, info, sizeof(*info), PROT_READ | PROT_WRITE) < 0) {
		(void)proc_put(proc);
		return -EINVAL;
	}

	err = proc_schedInfo(proc, policy, info);

	(void)proc_put(proc);

	return err;
}


/*
 * System state info
 */


int syscalls_threadsinfo(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int n, i;
	pid_t ppid;
	threadinfo_t *info;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, n, 0U);
				GETFROMSTACK(ustack, threadinfo_t *, info, 1U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, info, sizeof(*info) * (size_t)n, PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	n = proc_threadsList(n, info);

	for (i = 0; i < n; ++i) {
		ppid = posix_getppid(info[i].pid);
		if (ppid > 0) {
			info[i].ppid = ppid;
		}
	}

	return n;
}


void syscalls_meminfo(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	meminfo_t *info;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, meminfo_t *, info, 0U);
			},
			{
				return;
			});

	if (vm_mapBelongs(proc, info, sizeof(*info), PROT_READ | PROT_WRITE) >= 0) {
		vm_meminfo(info);
	}
}


int syscalls_syspageprog(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int i;
	size_t sz;
	syspageprog_t *prog;
	const syspage_prog_t *progSys;
	const char *name;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, syspageprog_t *, prog, 0U);
				GETFROMSTACK(ustack, int, i, 1U);
			},
			{
				return -EFAULT;
			});

	if ((i >= 0) && (vm_mapBelongs(proc, prog, sizeof(*prog), PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	sz = syspage_progSize();
	if (i < 0) {
		return (int)sz;
	}

	if (i >= (int)sz) {
		return -EINVAL;
	}

	progSys = syspage_progIdResolve((unsigned int)i);
	if (progSys == NULL) {
		return -EINVAL;
	}

	/* TODO: change syspageprog_t to allocate data for name dynamically */

	name = progSys->argv;
	for (sz = 0U; (name[sz] != '\0') && (name[sz] != ';'); ++sz) {
	}

	sz = min((sizeof(prog->name) - 1U), sz);
	if (*name == 'X') {
		name++;
		sz--;
	}

	USERMEM_TRY(
			{
				prog->addr = progSys->start;
				prog->size = progSys->end - progSys->start;
				hal_memcpy(prog->name, name, sz);
				prog->name[sz] = '\0';
			},
			{
				return -EFAULT;
			});

	return EOK;
}


int syscalls_sys_perf_start(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int mode;
	unsigned int flags;
	void *arg;
	size_t sz;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, mode, 0U);
				GETFROMSTACK(ustack, unsigned int, flags, 1U);
				GETFROMSTACK(ustack, void *, arg, 2U);
				GETFROMSTACK(ustack, size_t, sz, 3U);
			},
			{
				return -EFAULT;
			});

	if (arg != NULL && vm_mapBelongs(proc, arg, sz, PROT_READ) < 0) {
		return -EFAULT;
	}

	if (mode < 0 || mode >= (int)perf_mode_count) {
		return -ENOSYS;
	}

	return perf_start((perf_mode_t)mode, flags, arg, sz);
}


int syscalls_sys_perf_read(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	void *buffer;
	size_t sz;
	int mode, chan;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, mode, 0U);
				GETFROMSTACK(ustack, void *, buffer, 1U);
				GETFROMSTACK(ustack, size_t, sz, 2U);
				GETFROMSTACK(ustack, int, chan, 3U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, buffer, sz, PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	if (mode < 0 || mode >= (int)perf_mode_count) {
		return -ENOSYS;
	}

	return perf_read((perf_mode_t)mode, buffer, sz, chan);
}


int syscalls_sys_perf_stop(u8 *ustack)
{
	int mode;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, mode, 0U);
			},
			{
				return -EFAULT;
			});

	if (mode < 0 || mode >= (int)perf_mode_count) {
		return -ENOSYS;
	}

	return perf_stop((perf_mode_t)mode);
}


int syscalls_sys_perf_finish(u8 *ustack)
{
	int mode;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, mode, 0U);
			},
			{
				return -EFAULT;
			});

	if (mode < 0 || mode >= (int)perf_mode_count) {
		return -ENOSYS;
	}

	return perf_finish((perf_mode_t)mode);
}


/*
 * Mutexes
 */


int syscalls_phMutexCreate(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	handle_t *h;
	const struct lockAttr *attr;
	struct lockAttr kattr;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, handle_t *, h, 0U);
				GETFROMSTACK(ustack, const struct lockAttr *, attr, 1U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, h, sizeof(*h), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	if (vm_mapBelongs(proc, attr, sizeof(*attr), PROT_READ) < 0) {
		return -EFAULT;
	}

	USERMEM_TRY(
			{
				kattr = *attr;
			},
			{
				return -EFAULT;
			});

	res = proc_mutexCreate(&kattr);

	if (res < 0) {
		return res;
	}

	USERMEM_TRY(
			{
				*h = res;
			},
			{
				(void)proc_resourceDestroy(proc, res);
				return -EFAULT;
			});

	return EOK;
}


int syscalls_phMutexLock(u8 *ustack)
{
	handle_t h;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, handle_t, h, 0U);
			},
			{
				return -EFAULT;
			});
	return proc_mutexLock(h);
}


int syscalls_mutexTry(u8 *ustack)
{
	handle_t h;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, handle_t, h, 0U);
			},
			{
				return -EFAULT;
			});
	return proc_mutexTry(h);
}


int syscalls_mutexUnlock(u8 *ustack)
{
	handle_t h;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, handle_t, h, 0U);
			},
			{
				return -EFAULT;
			});
	return proc_mutexUnlock(h);
}


/*
 * Conditional variables
 */


int syscalls_phCondCreate(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	const struct condAttr *attr;
	struct condAttr kattr;
	handle_t *h;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, handle_t *, h, 0U);
				GETFROMSTACK(ustack, const struct condAttr *, attr, 1U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, h, sizeof(*h), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	if (vm_mapBelongs(proc, attr, sizeof(*attr), PROT_READ) < 0) {
		return -EFAULT;
	}

	USERMEM_TRY(
			{
				kattr = *attr;
			},
			{
				return -EFAULT;
			});

	res = proc_condCreate(&kattr);
	if (res < 0) {
		return res;
	}

	USERMEM_TRY(
			{
				*h = res;
			},
			{
				(void)proc_resourceDestroy(proc, res);
				return -EFAULT;
			});

	return EOK;
}


int syscalls_phCondWait(u8 *ustack)
{
	handle_t h;
	handle_t m;
	time_t timeout;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, handle_t, h, 0U);
				GETFROMSTACK(ustack, handle_t, m, 1U);
				GETFROMSTACK(ustack, time_t, timeout, 2U);
			},
			{
				return -EFAULT;
			});

	return proc_condWait(h, m, timeout);
}


int syscalls_condSignal(u8 *ustack)
{
	handle_t h;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, handle_t, h, 0U);
			},
			{
				return -EFAULT;
			});
	return proc_condSignal(h);
}


int syscalls_condBroadcast(u8 *ustack)
{
	handle_t h;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, handle_t, h, 0U);
			},
			{
				return -EFAULT;
			});
	return proc_condBroadcast(h);
}


/*
 * Resources
 */


int syscalls_resourceDestroy(u8 *ustack)
{
	handle_t h;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, handle_t, h, 0U);
			},
			{
				return -EFAULT;
			});
	return proc_resourceDestroy(proc_current()->process, h);
}


/*
 * Interrupt management
 */


int syscalls_interrupt(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	unsigned int n;
	userintrFn_t f;
	void *data;
	handle_t cond;
	handle_t *handle;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, unsigned int, n, 0U);
				GETFROMSTACK(ustack, userintrFn_t, f, 1U);
				GETFROMSTACK(ustack, void *, data, 2U);
				GETFROMSTACK(ustack, handle_t, cond, 3U);
				GETFROMSTACK(ustack, handle_t *, handle, 4U);
			},
			{
				return -EFAULT;
			});

	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1-a-2 "We want to check if at least start of memory occupied by the function is accessible to user." */
	if ((f == NULL) || (vm_mapBelongs(proc, (const void *)f, 1, PROT_READ | PROT_EXEC) < 0)) {
		return -EINVAL;
	}

	if ((handle != NULL) && (vm_mapBelongs(proc, handle, sizeof(*handle), PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	res = userintr_setHandler(n, f, data, cond);
	if (res < 0) {
		return res;
	}

	if (handle != NULL) {
		USERMEM_TRY(
				{
					*handle = res;
				},
				{
					(void)proc_resourceDestroy(proc, res);
					return -EFAULT;
				});
	}

	return EOK;
}


/*
 * Message passing
 */


int syscalls_portCreate(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	u32 *port;
	u32 kport;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, u32 *, port, 0U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, port, sizeof(*port), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	res = proc_portCreate(&kport);
	if (res != EOK) {
		return res;
	}

	USERMEM_TRY(
			{
				*port = kport;
			},
			{
				proc_portDestroy(kport);
				return -EFAULT;
			});

	return EOK;
}


void syscalls_portDestroy(u8 *ustack)
{
	u32 port;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, u32, port, 0U);
			},
			{
				return;
			});

	proc_portDestroy(port);
}


int syscalls_sys_portRegister(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	u32 port;
	const char *name;
	size_t len;
	const oid_t *oid;
	oid_t koid;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, u32, port, 0U);
				GETFROMSTACK(ustack, const char *, name, 1U);
				GETFROMSTACK(ustack, size_t, len, 2U);
				GETFROMSTACK(ustack, oid_t *, oid, 3U);
			},
			{
				return -EFAULT;
			});

	if (name == NULL || len == 0U) {
		return -EINVAL;
	}

	if ((oid != NULL) && (vm_mapBelongs(proc, oid, sizeof(*oid), PROT_READ) < 0)) {
		return -EFAULT;
	}

	if (vm_mapBelongs(proc, name, len, PROT_READ) < 0) {
		return -EFAULT;
	}

	if (oid == NULL) {
		return proc_portRegister(port, name, NULL);
	}
	if (usermem_memcpy(&koid, oid, sizeof(koid)) < 0) {
		return -EFAULT;
	}
	return proc_portRegister(port, name, &koid);
}


int syscalls_sys_portUnregister(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	const char *name;
	size_t len;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, const char *, name, 0U);
				GETFROMSTACK(ustack, size_t, len, 1U);
			},
			{
				return -EFAULT;
			});

	if (name == NULL || len == 0U) {
		return -EINVAL;
	}

	if (vm_mapBelongs(proc, name, len, PROT_READ) < 0) {
		return -EFAULT;
	}

	return proc_portUnregister(name);
}


int syscalls_msgSend(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	u32 port;
	msg_t *msg;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, u32, port, 0U);
				GETFROMSTACK(ustack, msg_t *, msg, 1U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, msg, sizeof(*msg), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}
#ifdef NOMMU /* MMU checks after copy, NOMMU is optimized to make no copy */
	if (msg->i.data != NULL) {
		if (vm_mapBelongs(proc, msg->i.data, msg->i.size, PROT_READ) < 0) {
			return -EFAULT;
		}
	}

	if (msg->o.data != NULL) {
		if (vm_mapBelongs(proc, msg->o.data, msg->o.size, PROT_READ | PROT_WRITE) < 0) {
			return -EFAULT;
		}
	}
#endif

	return proc_send(port, msg);
}


int syscalls_msgRecv(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	u32 port;
	msg_t *msg;
	msg_rid_t *rid, krid;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, u32, port, 0U);
				GETFROMSTACK(ustack, msg_t *, msg, 1U);
				GETFROMSTACK(ustack, msg_rid_t *, rid, 2U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, msg, sizeof(*msg), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	if (vm_mapBelongs(proc, rid, sizeof(*rid), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}
	USERMEM_TRY({ krid = *rid; }, { return -EFAULT; });

	res = proc_recv(port, msg, &krid);
	USERMEM_TRY({ *rid = krid; }, { return -EFAULT; });
	return res;
}


int syscalls_msgRespond(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	u32 port;
	const msg_t *msg;
	msg_rid_t rid;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, u32, port, 0U);
				GETFROMSTACK(ustack, msg_t *, msg, 1U);
				GETFROMSTACK(ustack, msg_rid_t, rid, 2U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, msg, sizeof(*msg), PROT_READ) < 0) {
		return -EFAULT;
	}

	return proc_respond(port, msg, rid);
}


int syscalls_lookup(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	char *name;
	oid_t *file, *dev;
	oid_t kfile, kdev;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, char *, name, 0U);
				GETFROMSTACK(ustack, oid_t *, file, 1U);
				GETFROMSTACK(ustack, oid_t *, dev, 2U);
			},
			{
				return -EFAULT;
			});

	if ((file != NULL) && (vm_mapBelongs(proc, file, sizeof(*file), PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	if ((dev != NULL) && (vm_mapBelongs(proc, dev, sizeof(*dev), PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	/* Ensure the string starts in process memory, as strndup also accepts strings fully-contained in kernel */
	if (vm_mapBelongs(proc, name, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}

	res = proc_portLookup(name, (file != NULL) ? &kfile : NULL, (dev != NULL) ? &kdev : NULL);

	if (res != EOK) {
		return res;
	}

	if (file != NULL) {
		USERMEM_TRY({ *file = kfile; }, { return -EFAULT; });
	}

	if (dev != NULL) {
		USERMEM_TRY({ *dev = kdev; }, { return -EFAULT; });
	}

	return EOK;
}


/*
 * Time management
 */


int syscalls_gettime(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	time_t *praw, *poffs;
	time_t kraw, koffs;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, time_t *, praw, 0U);
				GETFROMSTACK(ustack, time_t *, poffs, 1U);
			},
			{
				return -EFAULT;
			});

	if ((praw != NULL) && (vm_mapBelongs(proc, praw, sizeof(*praw), PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	if ((poffs != NULL) && (vm_mapBelongs(proc, poffs, sizeof(*poffs), PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	proc_gettime((praw != NULL) ? &kraw : NULL, (poffs != NULL) ? &koffs : NULL);

	if (praw != NULL) {
		USERMEM_TRY({ *praw = kraw; }, { return -EFAULT; });
	}

	if (poffs != NULL) {
		USERMEM_TRY({ *poffs = koffs; }, { return -EFAULT; });
	}

	return EOK;
}


int syscalls_settime(u8 *ustack)
{
	time_t offs;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, time_t, offs, 0U);
			},
			{
				return -EFAULT;
			});

	return proc_settime(offs);
}


/*
 * Power management
 */


void syscalls_keepidle(u8 *ustack)
{
	int t;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, t, 0U);
			},
			{
				return;
			});

	hal_cpuSetDevBusy(t);
}


/*
 * Platform specific call
 */


int syscalls_platformctl(u8 *ustack)
{
	/* FIXME: Allow access to sizeof(platformctl_t) to allow checks */
	void *ptr;
	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, void *, ptr, 0U);
			},
			{
				return -EFAULT;
			});
	return hal_platformctl(ptr);
}


/*
 * Watchdog
 */


void syscalls_wdgreload(u8 *ustack)
{
	hal_wdgReload();
}


addr_t syscalls_va2pa(u8 *ustack)
{
	void *va;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, void *, va, 0U);
			},
			{
				return 0;
			});

	return (pmap_resolve(proc_current()->process->pmapp, (void *)((ptr_t)va & ~0xfffU)) & ~0xfffU) + ((ptr_t)va & 0xfffU);
}


int syscalls_signalHandle(u8 *ustack)
{
	sighandlerFn_t handler;
	thread_t *thread;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, sighandlerFn_t, handler, 0U);
			},
			{
				return -EFAULT;
			});

	thread = proc_current();
	thread->process->sighandler = handler;

	return EOK;
}


int syscalls_signalPost(u8 *ustack)
{
	int pid, tid, signal, err;
	process_t *proc;
	thread_t *t = NULL;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, pid, 0U);
				GETFROMSTACK(ustack, int, tid, 1U);
				GETFROMSTACK(ustack, int, signal, 2U);
			},
			{
				return -EFAULT;
			});

	proc = proc_find(pid);
	if (proc == NULL) {
		return -EINVAL;
	}

	if (tid >= 0) {
		t = threads_findThread(tid);
		if (t == NULL) {
			(void)proc_put(proc);
			return -EINVAL;
		}
	}

	if ((t != NULL) && (t->process != proc)) {
		(void)proc_put(proc);
		threads_put(t);
		return -EINVAL;
	}

	err = threads_sigpost(proc, t, signal);

	(void)proc_put(proc);
	if (t != NULL) {
		threads_put(t);
	}

	return err;
}


unsigned int syscalls_signalMask(u8 *ustack)
{
	unsigned int mask, mmask, old;
	thread_t *t;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, unsigned int, mask, 0U);
				GETFROMSTACK(ustack, unsigned int, mmask, 1U);
			},
			{
				return 0U;
			});

	t = proc_current();

	old = t->sigmask;
	t->sigmask = (mask & mmask) | (t->sigmask & ~mmask);

	return old;
}


int syscalls_signalSuspend(u8 *ustack)
{
	unsigned int mask;
	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, unsigned int, mask, 0U);
			},
			{
				return -EFAULT;
			});

	return threads_sigsuspend(mask);
}


void syscalls_sigreturn(u8 *ustack)
{
	thread_t *t = proc_current();
	cpu_context_t *ctx;
	unsigned int oldmask;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, unsigned int, oldmask, 0U);
				GETFROMSTACK(ustack, cpu_context_t *, ctx, 1U);
			},
			{
				proc_kill(t->process);
				return;
			});

	if (ctx == NULL || (vm_mapBelongs(t->process, ctx, sizeof(*ctx), PROT_READ | PROT_WRITE) < 0)) {
		proc_kill(t->process);
		return;
	}

	hal_cpuDisableInterrupts();
	USERMEM_TRY(
			{
				hal_cpuSigreturn(t->kstack + t->kstacksz, ustack, &ctx);
			},
			{
				proc_kill(t->process);
				return;
			});

	t->sigmask = oldmask;

	/* TODO: check if return address belongs to user mapped memory */
	if (hal_cpuSupervisorMode(ctx) != 0) {
		proc_kill(t->process);
		return;
	}

	proc_longjmp(ctx);
}

/* POSIX compatibility syscalls */


int syscalls_sys_open(u8 *ustack)
{
	const char *filename;
	int oflag;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, const char *, filename, 0U);
				GETFROMSTACK(ustack, int, oflag, 1U);
			},
			{
				return -EFAULT;
			});

	/* Ensure the string starts in process memory, as strndup also accepts strings fully-contained in kernel */
	if (vm_mapBelongs(proc_current()->process, filename, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}

	return posix_open(filename, oflag, ustack);
}


int syscalls_sys_close(u8 *ustack)
{
	int fildes;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fildes, 0U);
			},
			{
				return -EFAULT;
			});

	return posix_close(fildes);
}


ssize_t syscalls_sys_read(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fildes;
	void *buf;
	size_t nbyte;
	off_t offset;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fildes, 0U);
				GETFROMSTACK(ustack, void *, buf, 1U);
				GETFROMSTACK(ustack, size_t, nbyte, 2U);
				GETFROMSTACK(ustack, off_t, offset, 3U);
			},
			{
				return -EFAULT;
			});

	if ((buf == NULL) && (nbyte != 0U)) {
		return -EFAULT;
	}

	if ((buf != NULL) && (nbyte != 0U) && (vm_mapBelongs(proc, buf, nbyte, PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	return posix_read(fildes, buf, nbyte, offset);
}


ssize_t syscalls_sys_write(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fildes;
	void *buf;
	size_t nbyte;
	off_t offset;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fildes, 0U);
				GETFROMSTACK(ustack, void *, buf, 1U);
				GETFROMSTACK(ustack, size_t, nbyte, 2U);
				GETFROMSTACK(ustack, off_t, offset, 3U);
			},
			{
				return -EFAULT;
			});

	if ((buf == NULL) && (nbyte != 0U)) {
		return -EFAULT;
	}

	if ((buf != NULL) && (nbyte != 0U) && (vm_mapBelongs(proc, buf, nbyte, PROT_READ) < 0)) {
		return -EFAULT;
	}

	return posix_write(fildes, buf, nbyte, offset);
}


int syscalls_sys_dup(u8 *ustack)
{
	int fildes;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fildes, 0U);
			},
			{
				return -EFAULT;
			});

	return posix_dup(fildes);
}


int syscalls_sys_dup2(u8 *ustack)
{
	int fildes;
	int fildes2;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fildes, 0U);
				GETFROMSTACK(ustack, int, fildes2, 1U);
			},
			{
				return -EFAULT;
			});

	return posix_dup2(fildes, fildes2);
}


int syscalls_sys_link(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	const char *path1;
	const char *path2;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, const char *, path1, 0U);
				GETFROMSTACK(ustack, const char *, path2, 1U);
			},
			{
				return -EFAULT;
			});

	/* Ensure the strings start in process memory, as strndup also accepts strings fully-contained in kernel */
	if ((vm_mapBelongs(proc, path1, 1U, PROT_READ) < 0) || (vm_mapBelongs(proc, path2, 1U, PROT_READ) < 0)) {
		return -EFAULT;
	}

	return posix_link(path1, path2);
}


int syscalls_sys_unlink(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	const char *pathname;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, const char *, pathname, 0U);
			},
			{
				return -EFAULT;
			});

	/* Ensure the string starts in process memory, as strndup also accepts strings fully-contained in kernel */
	if (vm_mapBelongs(proc, pathname, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}

	return posix_unlink(pathname);
}


int syscalls_sys_lseek(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fildes;
	off_t *offset, koffset;
	int whence;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fildes, 0U);
				GETFROMSTACK(ustack, off_t *, offset, 1U);
				GETFROMSTACK(ustack, int, whence, 2U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, offset, sizeof(*offset), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	USERMEM_TRY({ koffset = *offset; }, { return -EFAULT; });

	res = posix_lseek(fildes, &koffset, whence);

	if (res >= 0) {
		USERMEM_TRY({ *offset = koffset; }, { return -EFAULT; });
	}
	return res;
}


int syscalls_sys_ftruncate(u8 *ustack)
{
	int fildes;
	off_t length;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fildes, 0U);
				GETFROMSTACK(ustack, off_t, length, 1U);
			},
			{
				return -EFAULT;
			});

	return posix_ftruncate(fildes, length);
}


int syscalls_sys_fcntl(u8 *ustack)
{
	int fd;
	unsigned int cmd;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fd, 0U);
				GETFROMSTACK(ustack, unsigned int, cmd, 1U);
			},
			{
				return -EFAULT;
			});

	return posix_fcntl(fd, cmd, ustack);
}


int syscalls_sys_pipe(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int *fildes, kfildes[2];
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int *, fildes, 0U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, fildes, sizeof(*fildes) * 2U, PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	res = posix_pipe(kfildes);
	if (res >= 0) {
		USERMEM_TRY({ hal_memcpy(fildes, kfildes, sizeof(*fildes) * 2U); }, { return -EFAULT; });
	}

	return res;
}


int syscalls_sys_mkfifo(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	const char *path;
	mode_t mode;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, const char *, path, 0U);
				GETFROMSTACK(ustack, mode_t, mode, 1U);
			},
			{
				return -EFAULT;
			});

	/* Ensure the string starts in process memory, as strndup also accepts strings fully-contained in kernel */
	if (vm_mapBelongs(proc, path, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}

	res = posix_mkfifo(path, mode);

	return res;
}


int syscalls_sys_fstat(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fd;
	struct stat *buf;
	struct stat kbuf;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fd, 0U);
				GETFROMSTACK(ustack, struct stat *, buf, 1U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, buf, sizeof(*buf), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	res = posix_fstat(fd, &kbuf);
	if (res < 0) {
		return res;
	}

	USERMEM_TRY({ hal_memcpy(buf, &kbuf, sizeof(*buf)); }, { return -EFAULT; });

	return res;
}


int syscalls_sys_statvfs(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fd;
	const char *path;
	struct statvfs *buf;
	struct statvfs kbuf;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, const char *, path, 0U);
				GETFROMSTACK(ustack, int, fd, 1U);
				GETFROMSTACK(ustack, struct statvfs *, buf, 2U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, buf, sizeof(*buf), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	if (path != NULL) {
		/* Ensure the string starts in process memory, as strndup also accepts strings fully-contained in kernel */
		if (vm_mapBelongs(proc, path, 1U, PROT_READ) < 0) {
			return -EFAULT;
		}
	}

	res = posix_statvfs(path, fd, &kbuf);

	if (res < 0) {
		return res;
	}

	USERMEM_TRY({ hal_memcpy(buf, &kbuf, sizeof(*buf)); }, { return -EFAULT; });

	return res;
}


int syscalls_sys_fsync(u8 *ustack)
{
	int fd;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fd, 0U);
			},
			{
				return -EFAULT;
			});

	return posix_fsync(fd);
}


int syscalls_sys_chmod(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	const char *path;
	mode_t mode;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, const char *, path, 0U);
				GETFROMSTACK(ustack, mode_t, mode, 1U);
			},
			{
				return -EFAULT;
			});

	/* Ensure the string starts in process memory, as strndup also accepts strings fully-contained in kernel */
	if (vm_mapBelongs(proc, path, 1U, PROT_READ) < 0) {
		return -EFAULT;
	}

	return posix_chmod(path, mode);
}


int syscalls_sys_accept(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;
	socklen_t alen;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, struct sockaddr *, address, 1U);
				GETFROMSTACK(ustack, socklen_t *, address_len, 2U);
			},
			{
				return -EFAULT;
			});

	if (address != NULL) {
		if (vm_mapBelongs(proc, address_len, sizeof(*address_len), PROT_READ | PROT_WRITE) < 0) {
			return -EFAULT;
		}

		USERMEM_TRY({ alen = *address_len; }, { return -EFAULT; });

		if (vm_mapBelongs(proc, address, alen, PROT_READ | PROT_WRITE) < 0) {
			return -EFAULT;
		}
	}

	res = posix_accept(socket, address, address != NULL ? &alen : NULL);

	if (address != NULL) {
		USERMEM_TRY({ *address_len = alen; }, { return -EFAULT; });
	}
	return res;
}


int syscalls_sys_accept4(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;
	socklen_t alen;
	int flags, res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, struct sockaddr *, address, 1U);
				GETFROMSTACK(ustack, socklen_t *, address_len, 2U);
				GETFROMSTACK(ustack, int, flags, 3U);
			},
			{
				return -EFAULT;
			});

	if (address != NULL) {
		if (vm_mapBelongs(proc, address_len, sizeof(*address_len), PROT_READ | PROT_WRITE) < 0) {
			return -EFAULT;
		}

		USERMEM_TRY({ alen = *address_len; }, { return -EFAULT; });

		if (vm_mapBelongs(proc, address, alen, PROT_READ | PROT_WRITE) < 0) {
			return -EFAULT;
		}
	}

	res = posix_accept4(socket, address, address != NULL ? &alen : NULL, flags);

	if (address != NULL) {
		USERMEM_TRY({ *address_len = alen; }, { return -EFAULT; });
	}
	return res;
}


int syscalls_sys_bind(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	const struct sockaddr *address;
	socklen_t address_len;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, const struct sockaddr *, address, 1U);
				GETFROMSTACK(ustack, socklen_t, address_len, 2U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, address, address_len, PROT_READ) < 0) {
		return -EFAULT;
	}

	return posix_bind(socket, address, address_len);
}


int syscalls_sys_connect(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	const struct sockaddr *address;
	socklen_t address_len;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, const struct sockaddr *, address, 1U);
				GETFROMSTACK(ustack, socklen_t, address_len, 2U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, address, address_len, PROT_READ) < 0) {
		return -EFAULT;
	}

	return posix_connect(socket, address, address_len);
}


int syscalls_sys_gethostname(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	char *name;
	size_t namelen;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, char *, name, 0U);
				GETFROMSTACK(ustack, size_t, namelen, 1U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, name, namelen, PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	return posix_gethostname(name, namelen);
}


int syscalls_sys_getpeername(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;
	socklen_t alen;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, struct sockaddr *, address, 1U);
				GETFROMSTACK(ustack, socklen_t *, address_len, 2U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, address_len, sizeof(*address_len), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	USERMEM_TRY({ alen = *address_len; }, { return -EFAULT; });

	if (vm_mapBelongs(proc, address, alen, PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	res = posix_getpeername(socket, address, &alen);
	USERMEM_TRY({ *address_len = alen; }, { return -EFAULT; });
	return res;
}


int syscalls_sys_getsockname(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	struct sockaddr *address;
	socklen_t *address_len;
	socklen_t alen;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, struct sockaddr *, address, 1U);
				GETFROMSTACK(ustack, socklen_t *, address_len, 2U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, address_len, sizeof(*address_len), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	USERMEM_TRY({ alen = *address_len; }, { return -EFAULT; });

	if (vm_mapBelongs(proc, address, alen, PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	res = posix_getsockname(socket, address, &alen);
	USERMEM_TRY({ *address_len = alen; }, { return -EFAULT; });
	return res;
}


int syscalls_sys_getsockopt(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	int level;
	int optname;
	void *optval;
	socklen_t *optlen;
	socklen_t olen = 0U;
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, int, level, 1U);
				GETFROMSTACK(ustack, int, optname, 2U);
				GETFROMSTACK(ustack, void *, optval, 3U);
				GETFROMSTACK(ustack, socklen_t *, optlen, 4U);
			},
			{
				return -EFAULT;
			});

	if (optval != NULL) {
		if (vm_mapBelongs(proc, optlen, sizeof(*optlen), PROT_READ | PROT_WRITE) < 0) {
			return -EFAULT;
		}

		USERMEM_TRY({ olen = *optlen; }, { return -EFAULT; });

		if (vm_mapBelongs(proc, optval, olen, PROT_READ | PROT_WRITE) < 0) {
			return -EFAULT;
		}
	}

	res = posix_getsockopt(socket, level, optname, optval, &olen);

	if (optval != NULL) {
		USERMEM_TRY({ *optlen = olen; }, { return -EFAULT; });
	}
	return res;
}


int syscalls_sys_listen(u8 *ustack)
{
	int socket;
	int backlog;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, int, backlog, 1U);
			},
			{
				return -EFAULT;
			});

	return posix_listen(socket, backlog);
}


ssize_t syscalls_sys_recvfrom(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	void *message;
	size_t length;
	int flags;
	struct sockaddr *src_addr;
	socklen_t *src_len, slen;
	ssize_t res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, void *, message, 1U);
				GETFROMSTACK(ustack, size_t, length, 2U);
				GETFROMSTACK(ustack, int, flags, 3U);
				GETFROMSTACK(ustack, struct sockaddr *, src_addr, 4U);
				GETFROMSTACK(ustack, socklen_t *, src_len, 5U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, message, length, PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	if (src_addr != NULL) {
		if (vm_mapBelongs(proc, src_len, sizeof(*src_len), PROT_READ | PROT_WRITE) < 0) {
			return -EFAULT;
		}

		USERMEM_TRY({ slen = *src_len; }, { return -EFAULT; });

		if (vm_mapBelongs(proc, src_addr, slen, PROT_READ | PROT_WRITE) < 0) {
			return -EFAULT;
		}
	}

	res = posix_recvfrom(socket, message, length, flags, src_addr, src_addr != NULL ? &slen : NULL);

	if (src_addr != NULL) {
		USERMEM_TRY({ *src_len = slen; }, { return -EFAULT; });
	}
	return res;
}


ssize_t syscalls_sys_sendto(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	const void *message;
	size_t length;
	int flags;
	const struct sockaddr *dest_addr;
	socklen_t dest_len;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, const void *, message, 1U);
				GETFROMSTACK(ustack, size_t, length, 2U);
				GETFROMSTACK(ustack, int, flags, 3U);
				GETFROMSTACK(ustack, const struct sockaddr *, dest_addr, 4U);
				GETFROMSTACK(ustack, socklen_t, dest_len, 5U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, message, length, PROT_READ) < 0) {
		return -EFAULT;
	}

	if ((dest_addr != NULL) && (vm_mapBelongs(proc, dest_addr, dest_len, PROT_READ) < 0)) {
		return -EFAULT;
	}

	return posix_sendto(socket, message, length, flags, dest_addr, dest_len);
}


ssize_t syscalls_sys_recvmsg(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	struct msghdr *msg, kmsg;
	int flags;
	size_t i;
	ssize_t res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, struct msghdr *, msg, 1U);
				GETFROMSTACK(ustack, int, flags, 2U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, msg, sizeof(*msg), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	if (usermem_memcpy(&kmsg, msg, sizeof(kmsg)) < 0) {
		return -EFAULT;
	}

	if ((kmsg.msg_iovlen != 0) && (vm_mapBelongs(proc, kmsg.msg_iov, sizeof(*kmsg.msg_iov) * (size_t)kmsg.msg_iovlen, PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	for (i = 0; i < (size_t)kmsg.msg_iovlen; ++i) {
		if ((kmsg.msg_iov[i].iov_base != NULL) && (vm_mapBelongs(proc, kmsg.msg_iov[i].iov_base, kmsg.msg_iov[i].iov_len, PROT_READ | PROT_WRITE) < 0)) {
			return -EFAULT;
		}
	}

	if ((kmsg.msg_control != NULL) && (vm_mapBelongs(proc, kmsg.msg_control, kmsg.msg_controllen, PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	if ((kmsg.msg_name != NULL) && (vm_mapBelongs(proc, kmsg.msg_name, kmsg.msg_namelen, PROT_READ | PROT_WRITE) < 0)) {
		return -EFAULT;
	}

	/* FIXME: potential TOCTOU on msg subfields */

	USERMEM_TRY(
			{
				res = posix_recvmsg(socket, &kmsg, flags);
				hal_memcpy(msg, &kmsg, sizeof(kmsg));
			},
			{
				return -EFAULT;
			});
	return res;
}


ssize_t syscalls_sys_sendmsg(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	const struct msghdr *msg;
	struct msghdr kmsg;
	int flags;
	size_t i;
	ssize_t res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, const struct msghdr *, msg, 1U);
				GETFROMSTACK(ustack, int, flags, 2U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, msg, sizeof(*msg), PROT_READ) < 0) {
		return -EFAULT;
	}

	if (usermem_memcpy(&kmsg, msg, sizeof(kmsg)) < 0) {
		return -EFAULT;
	}

	if ((kmsg.msg_iovlen != 0) && (vm_mapBelongs(proc, kmsg.msg_iov, sizeof(*kmsg.msg_iov) * (size_t)kmsg.msg_iovlen, PROT_READ) < 0)) {
		return -EFAULT;
	}

	for (i = 0; i < (size_t)kmsg.msg_iovlen; ++i) {
		if ((kmsg.msg_iov[i].iov_base != NULL) && (vm_mapBelongs(proc, kmsg.msg_iov[i].iov_base, kmsg.msg_iov[i].iov_len, PROT_READ) < 0)) {
			return -EFAULT;
		}
	}

	if ((kmsg.msg_control != NULL) && (vm_mapBelongs(proc, kmsg.msg_control, kmsg.msg_controllen, PROT_READ) < 0)) {
		return -EFAULT;
	}

	if ((kmsg.msg_name != NULL) && (vm_mapBelongs(proc, kmsg.msg_name, kmsg.msg_namelen, PROT_READ) < 0)) {
		return -EFAULT;
	}

	/* FIXME: potential TOCTOU on msg subfields */

	USERMEM_TRY(
			{
				res = posix_sendmsg(socket, &kmsg, flags);
			},
			{
				return -EFAULT;
			});
	return res;
}


int syscalls_sys_socket(u8 *ustack)
{
	int domain;
	int type;
	int protocol;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, domain, 0U);
				GETFROMSTACK(ustack, int, type, 1U);
				GETFROMSTACK(ustack, int, protocol, 2U);
			},
			{
				return -EFAULT;
			});

	return posix_socket(domain, type, protocol);
}


int syscalls_sys_socketpair(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int domain;
	int type;
	int protocol;
	int *sv, ksv[2];
	int res;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, domain, 0U);
				GETFROMSTACK(ustack, int, type, 1U);
				GETFROMSTACK(ustack, int, protocol, 2U);
				GETFROMSTACK(ustack, int *, sv, 3U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, sv, sizeof(*sv) * 2U, PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	res = posix_socketpair(domain, type, protocol, ksv);
	if (res == 0) {
		USERMEM_TRY({ hal_memcpy(sv, ksv, sizeof(*sv) * 2U); }, { return -EFAULT; });
	}
	return res;
}


int syscalls_sys_shutdown(u8 *ustack)
{
	int socket;
	int how;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, int, how, 1U);
			},
			{
				return -EFAULT;
			});

	return posix_shutdown(socket, how);
}


int syscalls_sys_sethostname(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	const char *name;
	size_t namelen;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, const char *, name, 0U);
				GETFROMSTACK(ustack, size_t, namelen, 1U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc, name, namelen, PROT_READ) < 0) {
		return -EFAULT;
	}

	return posix_sethostname(name, namelen);
}


int syscalls_sys_setsockopt(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int socket;
	int level;
	int optname;
	const void *optval;
	socklen_t optlen;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, socket, 0U);
				GETFROMSTACK(ustack, int, level, 1U);
				GETFROMSTACK(ustack, int, optname, 2U);
				GETFROMSTACK(ustack, const void *, optval, 3U);
				GETFROMSTACK(ustack, socklen_t, optlen, 4U);
			},
			{
				return -EFAULT;
			});

	if ((optval != NULL) && (optlen != 0U) && (vm_mapBelongs(proc, optval, optlen, PROT_READ) < 0)) {
		return -EFAULT;
	}

	return posix_setsockopt(socket, level, optname, optval, optlen);
}


int syscalls_sys_ioctl(u8 *ustack)
{
	int fildes;
	unsigned long request;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fildes, 0U);
				GETFROMSTACK(ustack, unsigned long, request, 1U);
			},
			{
				return -EFAULT;
			});

	/* vm_mapBelongs on optional data pointer checked in posix_ioctl */
	return posix_ioctl(fildes, request, ustack);
}


int syscalls_sys_poll(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	struct pollfd *fds;
	nfds_t nfds;
	int timeout_ms;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, struct pollfd *, fds, 0U);
				GETFROMSTACK(ustack, nfds_t, nfds, 1U);
				GETFROMSTACK(ustack, int, timeout_ms, 2U);
			},
			{
				return -EFAULT;
			});

	/* parasoft-suppress-next-line MISRAC2012-RULE_14_3-ac "Check needed on NOMMU + nfds_t type can change." */
	if (nfds > (size_t)-1 / sizeof(*fds)) {
		/* nfds * sizeof(*fds) would overflow */
		return -EINVAL;
	}

	if (vm_mapBelongs(proc, fds, nfds * sizeof(*fds), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	return posix_poll(fds, nfds, timeout_ms);
}


int syscalls_sys_futimens(u8 *ustack)
{
	process_t *proc = proc_current()->process;
	int fildes;
	const struct timespec *times;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, int, fildes, 0U);
				GETFROMSTACK(ustack, const struct timespec *, times, 1U);
			},
			{
				return -EFAULT;
			});

	if ((times != NULL) && (vm_mapBelongs(proc, times, 2U * sizeof(*times), PROT_READ) < 0)) {
		return -EFAULT;
	}

	return posix_futimens(fildes, times);
}


int syscalls_sys_tkill(u8 *ustack)
{
	pid_t pid;
	int tid;
	int sig;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, pid_t, pid, 0U);
				GETFROMSTACK(ustack, int, tid, 1U);
				GETFROMSTACK(ustack, int, sig, 2U);
			},
			{
				return -EFAULT;
			});

	return posix_tkill(pid, tid, sig);
}


int syscalls_sys_setpgid(u8 *ustack)
{
	pid_t pid, pgid;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, pid_t, pid, 0U);
				GETFROMSTACK(ustack, pid_t, pgid, 1U);
			},
			{
				return -EFAULT;
			});

	return posix_setpgid(pid, pgid);
}


pid_t syscalls_sys_getpgid(u8 *ustack)
{
	pid_t pid;

	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, pid_t, pid, 0U);
			},
			{
				return -EFAULT;
			});

	return posix_getpgid(pid);
}


int syscalls_sys_setpgrp(u8 *ustack)
{
	return posix_setpgid(0, 0);
}


pid_t syscalls_sys_getpgrp(u8 *ustack)
{
	return posix_getpgid(0);
}


pid_t syscalls_sys_setsid(u8 *ustack)
{
	return posix_setsid();
}


void syscalls_sbi_putchar(u8 *ustack)
{
#ifdef __TARGET_RISCV64
	char c;
	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, char, c, 0U);
			},
			{
				return;
			});
	(void)hal_sbiPutchar((int)c);
#endif
}


int syscalls_sbi_getchar(u8 *ustack)
{
#ifdef __TARGET_RISCV64
	return (int)hal_sbiGetchar();
#else
	return -1;
#endif
}


int syscalls_sys_uname(u8 *ustack)
{
	struct utsname *name;
	USERMEM_TRY(
			{
				GETFROMSTACK(ustack, struct utsname *, name, 0U);
			},
			{
				return -EFAULT;
			});

	if (vm_mapBelongs(proc_current()->process, name, sizeof(*name), PROT_READ | PROT_WRITE) < 0) {
		return -EFAULT;
	}

	return posix_uname(name);
}


/*
 * Empty syscall
 */


int syscalls_notimplemented(void)
{
	return -ENOTTY;
}


/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 "Syscalls are in different types" */
const void *const syscalls[] = { SYSCALLS(SYSCALLS_NAME) };


void *syscalls_dispatch(unsigned int n, u8 *ustack, cpu_context_t *ctx)
{
	void *retval;
	thread_t *thread;

	if (n >= sizeof(syscalls) / sizeof(syscalls[0])) {
		threads_setupUserReturn((void *)-EINVAL, ctx);
		return (void *)-EINVAL;
	}

	if (vm_mapBelongs(proc_current()->process, ustack, 1U, PROT_READ) < 0) {
		threads_setupUserReturn((void *)-EFAULT, ctx);
		return (void *)-EFAULT;
	}

	thread = proc_current();

	trace_eventSyscallEnter(n, proc_getTid(thread));

	/* parasoft-suppress-next-line MISRAC2012-RULE_11_1 MISRAC2012-RULE_11_8 "Related to previous suppression" */
	retval = ((void *(*)(u8 *arg))syscalls[n])(ustack);

	/* after forking child returns with same stack but in different thread */
	thread = proc_current();

	trace_eventSyscallExit(n, proc_getTid(thread));

	if (thread->exit != 0U) {
		proc_threadEnd();
	}

	threads_setupUserReturn(retval, ctx);

	return retval;
}


void _syscalls_init(void)
{
	lib_printf("syscalls: Initializing syscall table [%d]\n", sizeof(syscalls) / sizeof(syscalls[0]));
}

/* parasoft-end-suppress MISRAC2012-RULE_8_4 */
