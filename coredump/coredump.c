/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Kernel routines for userspace coredump server
 *
 * Copyright 2026 Phoenix Systems
 * Author: Jakub Klimek
 *
 * This file is part of Phoenix-RTOS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "coredump.h"

#include "hal/hal.h"
#include "proc/proc.h"
#include "vm/vm.h"
#include "include/errno.h"

#include <board_config.h>


/* Max number of crashed processes kept waiting before reaping for the coredump server */
#ifndef COREDUMP_QUEUE_MAX
#define COREDUMP_QUEUE_MAX 2U
#endif


static struct {
	spinlock_t spinlock;
	process_t *actServer; /* Currently dumping coredump server (weak ref) */
	thread_t *crashedq;   /* Crashed threads waiting to be dumped */
	size_t crashedqLen;
	thread_t *waitq; /* Server threads blocked in coredumpWait() */

	/* Serializes server requests, protects current and window */
	lock_t lock;
	thread_t *current; /* Crashed thread of the process being dumped */
	struct {           /* Crashed process memory mapped into the server by coredumpMap() */
		void *vaddr;   /* NULL if nothing to unmap */
		size_t size;
	} window;
} coredump_common;


/* Removes the current memory window from the server. Must be called with coredump_common.lock held */
static void _coredump_unmapWindow(process_t *server)
{
	if (coredump_common.window.vaddr != NULL) {
		(void)vm_munmap(server->mapp, coredump_common.window.vaddr, coredump_common.window.size);
		coredump_common.window.vaddr = NULL;
		coredump_common.window.size = 0;
	}
}


static void coredump_release(thread_t *crashed)
{
	(void)proc_put(crashed->process);
}


/* Detach the session of a dead server. Release the crashed process mid-dump. */
static void coredump_serverDied(process_t *server)
{
	thread_t *abandoned;
	spinlock_ctx_t sc;

	(void)proc_lockSet(&coredump_common.lock);
	hal_spinlockSet(&coredump_common.spinlock, &sc);
	coredump_common.actServer = NULL;
	hal_spinlockClear(&coredump_common.spinlock, &sc);

	/* Window maps pages of the abandoned process - remove it before they are freed */
	_coredump_unmapWindow(server);

	abandoned = coredump_common.current;
	coredump_common.current = NULL;
	(void)proc_lockClear(&coredump_common.lock);

	/* Only partially dumped process is released, crashes still in the queue wait for the next server */
	if (abandoned != NULL) {
		coredump_release(abandoned);
	}
}


static int coredump_isServer(process_t *process)
{
	spinlock_ctx_t sc;
	int ret;

	hal_spinlockSet(&coredump_common.spinlock, &sc);
	ret = (coredump_common.actServer == process) ? 1 : 0;
	hal_spinlockClear(&coredump_common.spinlock, &sc);

	return ret;
}


int coredump_enqueue(process_t *process)
{
	thread_t *thread, *ghost;
	spinlock_ctx_t sc;
	int err;

	/* No need to protect process.ghosts, there are no alive threads left */
	thread = process->ghosts;

	/* Cleanup threads that finished before crash */
	do {
		while ((process->ghosts != NULL) && (thread->kstack == NULL)) {
			ghost = thread;
			thread = thread->procnext;
			LIST_REMOVE_EX(&process->ghosts, ghost, procnext, procprev);
			vm_kfree(ghost);
		}
		if (process->ghosts == NULL) {
			return -ENOENT;
		}
		thread = thread->procnext;
	} while (thread != process->ghosts);

	/* Find crashing thread */
	do {
		if (thread->crashed != 0U) {
			break;
		}
		thread = thread->procnext;
	} while (thread != process->ghosts);

	if (thread->crashed == 0U) {
		return -ENOENT;
	}

	/* Avoid deadlock in case dumper is a client */
	proc_portsDestroy(process);

	if (coredump_isServer(process) != 0) {
		coredump_serverDied(process);
	}

	hal_spinlockSet(&coredump_common.spinlock, &sc);
	if (coredump_common.crashedqLen < COREDUMP_QUEUE_MAX) {
		LIST_ADD(&coredump_common.crashedq, thread);
		coredump_common.crashedqLen++;
		(void)proc_threadWakeup(&coredump_common.waitq);
		err = EOK;
	}
	else {
		err = -ENOSPC;
	}
	hal_spinlockClear(&coredump_common.spinlock, &sc);

	if (err != EOK) {
		lib_printf("coredump: queue full, dropping process %d (%s)\n", process_getPid(process), process->path == NULL ? "NULL path" : process->path);
	}

	return err;
}


void coredump_processDestroyed(process_t *process)
{
	if (coredump_isServer(process) != 0) {
		coredump_serverDied(process);
	}
}


/* Must be called with coredump_common.lock held */
static int _coredump_currentlyDumped(thread_t **crashed)
{
	process_t *caller = proc_current()->process;
	process_t *server;
	spinlock_ctx_t sc;

	hal_spinlockSet(&coredump_common.spinlock, &sc);
	server = coredump_common.actServer;
	hal_spinlockClear(&coredump_common.spinlock, &sc);

	if ((caller == NULL) || (server != caller)) {
		return -EPERM;
	}

	if (coredump_common.current == NULL) {
		return -ENOENT;
	}

	*crashed = coredump_common.current;

	return EOK;
}


static size_t coredump_memEntryList(process_t *process, coredump_memseg_t *list, size_t n)
{
	map_entry_t *e;
	size_t segCnt = 0;
	size_t i = 0;

	(void)proc_lockSet(&process->mapp->lock);

#ifdef NOMMU

	e = process->entries;
	do {
		if (((e->flags & MAP_DEVICE) == 0U) && ((e->prot & PROT_READ) != 0U) && ((e->prot & PROT_WRITE) != 0U)) {
			segCnt++;
			if (i < n) {
				list[i].startAddr = e->vaddr;
				list[i].endAddr = e->vaddr + e->size;
				i++;
			}
		}
		e = e->next;
	} while (e != process->entries);

#else

	e = lib_treeof(map_entry_t, linkage, lib_rbMinimum(process->mapp->tree.root));
	while (e != NULL) {
		if (((e->flags & MAP_DEVICE) == 0U) && ((e->prot & PROT_READ) != 0U) && ((e->prot & PROT_WRITE) != 0U)) {
			segCnt++;
			if (i < n) {
				list[i].startAddr = e->vaddr;
				list[i].endAddr = e->vaddr + e->size;
				i++;
			}
		}
		e = lib_treeof(map_entry_t, linkage, lib_rbNext(&e->linkage));
	}

#endif

	(void)proc_lockClear(&process->mapp->lock);

	return segCnt;
}


static size_t coredump_threadCnt(process_t *process)
{
	thread_t *t = process->ghosts;
	size_t count = 0;

	if (t == NULL) {
		return 0;
	}
	do {
		count++;
		t = t->procnext;
	} while (t != process->ghosts);

	return count;
}


static void coredump_generalInfo(thread_t *crashed, coredump_general_t *info)
{
	process_t *process = crashed->process;
	size_t pathlen;
	const char *path;

	info->pid = process_getPid(process);
	info->tid = proc_getTid(crashed);
	/* Crash reason - termination signal recorded by proc_crash() */
	info->signo = ((unsigned int)process->exit >> 8) & 0x7fU;
	info->memSegCnt = coredump_memEntryList(process, NULL, 0);
	info->threadCnt = coredump_threadCnt(process);
	info->type = (sizeof(void *) == 8U) ? coredumpType64 : coredumpType32;

	if (process->path != NULL) {
		path = process->path;
		pathlen = hal_strlen(path) + 1U;
		/* Keep the tail of the path if it doesn't fit */
		if (pathlen > sizeof(info->path)) {
			path += pathlen - sizeof(info->path);
			pathlen = sizeof(info->path);
		}
		hal_memcpy(info->path, path, pathlen);
	}
	else {
		info->path[0] = '\0';
	}
}


int coredump_wait(coredump_general_t *info)
{
	process_t *caller = proc_current()->process;
	thread_t *crashed;
	spinlock_ctx_t sc;
	int err = EOK;

	if ((caller == NULL) || (info == NULL)) {
		return -EINVAL;
	}

	if (vm_mapBelongs(caller, info, sizeof(*info)) < 0) {
		return -EFAULT;
	}


	hal_spinlockSet(&coredump_common.spinlock, &sc);
	if (coredump_common.actServer == NULL) {
		coredump_common.actServer = caller;
	}
	else {
		err = -EBUSY;
	}
	hal_spinlockClear(&coredump_common.spinlock, &sc);

	if (err != EOK) {
		return err;
	}

	hal_spinlockSet(&coredump_common.spinlock, &sc);
	while ((coredump_common.crashedq == NULL) && (err != -EINTR)) {
		err = proc_threadWaitInterruptible(&coredump_common.waitq, &coredump_common.spinlock, 0, &sc);
	}

	crashed = coredump_common.crashedq;
	if (crashed != NULL) {
		LIST_REMOVE(&coredump_common.crashedq, crashed);
		coredump_common.crashedqLen--;
	}
	else {
		coredump_common.actServer = NULL;
		hal_spinlockClear(&coredump_common.spinlock, &sc);
		return -EINTR;
	}
	hal_spinlockClear(&coredump_common.spinlock, &sc);

	(void)proc_lockSet(&coredump_common.lock);
	LIB_ASSERT(coredump_common.current == NULL, "session already open (dumping pid %d) during pid %d crash claim",
			process_getPid(coredump_common.current->process), process_getPid(crashed->process));
	coredump_common.current = crashed;
	coredump_generalInfo(crashed, info);
	err = EOK;
	(void)proc_lockClear(&coredump_common.lock);

	return err;
}


static int coredump_infoThread(thread_t *crashed, int tid, coredump_thread_t *info, size_t size)
{
	process_t *process = crashed->process;
	thread_t *thread = process->ghosts;

	if (size < (sizeof(coredump_thread_t) + sizeof(cpu_context_t))) {
		return -EINVAL;
	}

	do {
		if (proc_getTid(thread) == tid) {
			break;
		}
		thread = thread->procnext;
	} while (thread != process->ghosts);

	if (proc_getTid(thread) != tid) {
		return -ENOENT;
	}

	info->tid = tid;
	info->nextTid = proc_getTid(thread->procnext);
	info->stackAddr = thread->ustack;

	return hal_cpuGetUserCtx(thread->context, (char *)thread->kstack + thread->kstacksz, process->pmapp, (cpu_context_t *)info->context);
}


static int coredump_infoRelocs(thread_t *crashed, coredump_reloc_t *relocs, size_t size)
{
	size_t i = 0;

#ifdef NOMMU

	process_t *process = crashed->process;
	size_t n = size / sizeof(coredump_reloc_t);

	for (; (i < process->relocsz) && (i < n); i++) {
		relocs[i].vbase = process->reloc[i].vbase;
		relocs[i].pbase = process->reloc[i].pbase;
	}

#else

	(void)crashed;

#endif

	if (size > (i * sizeof(coredump_reloc_t))) {
		hal_memset((char *)relocs + i * sizeof(coredump_reloc_t), 0, size - i * sizeof(coredump_reloc_t));
	}

	return (int)i;
}


int coredump_info(int type, int tid, void *buf, size_t size)
{
	process_t *caller = proc_current()->process;
	thread_t *crashed;
	int err;

	if ((caller == NULL) || ((buf == NULL) && (size != 0U))) {
		return -EINVAL;
	}

	if ((size != 0U) && (vm_mapBelongs(caller, buf, size) < 0)) {
		return -EFAULT;
	}

	(void)proc_lockSet(&coredump_common.lock);

	err = _coredump_currentlyDumped(&crashed);
	if (err == EOK) {
		switch (type) {
			case coredumpInfoThread:
				err = coredump_infoThread(crashed, tid, buf, size);
				break;

			case coredumpInfoMemList:
				err = (int)coredump_memEntryList(crashed->process, buf, size / sizeof(coredump_memseg_t));
				break;

			case coredumpInfoReloc:
				err = coredump_infoRelocs(crashed, buf, size);
				break;

			default:
				err = -EINVAL;
				break;
		}
	}

	(void)proc_lockClear(&coredump_common.lock);

	return err;
}


#ifndef NOMMU

static int _coredump_mapPages(process_t *crashed, process_t *server, const void *addr, size_t len, const void **ptr)
{
	vm_map_t *src = crashed->mapp;
	vm_map_t *dst = server->mapp;
	ptr_t start = (ptr_t)addr & ~(ptr_t)(SIZE_PAGE - 1U);
	size_t size = round_page((ptr_t)addr + len) - start;
	size_t offs;
	addr_t pa;
	int flags, err = EOK;
	void *w;

	/* Reserve server address space, pages are mapped directly - like in msg_map() */
	w = vm_mapFind(dst, NULL, size, MAP_NOINHERIT, PROT_READ | PROT_USER);
	if (w == NULL) {
		return -ENOMEM;
	}

	/* Whole pages are mapped - the server may see all of crashed process memory anyway */
	for (offs = 0; offs < size; offs += SIZE_PAGE) {
		/* TODO: Assuming lazy=0, all mapped pages are present */
		pa = pmap_resolve(&src->pmap, (void *)(start + offs)) & ~(addr_t)(SIZE_PAGE - 1U);
		flags = vm_mapFlags(src, (void *)(start + offs));
		if ((pa == 0U) || (flags < 0)) {
			err = -EFAULT;
			break;
		}

		if (((vm_flags_t)flags & MAP_DEVICE) != 0U) {
			err = -EINVAL;
			break;
		}

		if (page_map(&dst->pmap, (u8 *)w + offs, pa, PGHD_READ | PGHD_PRESENT | PGHD_USER | vm_flagsToAttr((vm_flags_t)flags)) < 0) {
			err = -ENOMEM;
			break;
		}
	}

	if (err != EOK) {
		(void)vm_munmap(dst, w, size);
		return err;
	}

	coredump_common.window.vaddr = w;
	coredump_common.window.size = size;
	*ptr = (const u8 *)w + ((ptr_t)addr - start);

	return EOK;
}

#else

static int _coredump_mapPages(process_t *crashed, process_t *server, const void *addr, size_t len, const void **ptr)
{
	void *w;

	(void)crashed;

	/* Single address space - pass the memory directly if the server is allowed to access it */
	if (pmap_isAllowed(server->pmapp, addr, len) != 0) {
		*ptr = addr;
		return EOK;
	}

	/* Otherwise copy into server memory - like msg-nommu does */
	w = vm_mmap(server->mapp, NULL, NULL, round_page(len), PROT_READ | PROT_USER, NULL, -1, MAP_ANONYMOUS);
	if (w == NULL) {
		return -ENOMEM;
	}
	hal_memcpy(w, addr, len);

	coredump_common.window.vaddr = w;
	coredump_common.window.size = round_page(len);
	*ptr = w;

	return EOK;
}

#endif


int coredump_map(const void *addr, size_t len, const void **ptr)
{
	process_t *caller = proc_current()->process;
	thread_t *crashed;
	const void *mapped = NULL;
	int err;

	if ((caller == NULL) || (ptr == NULL) || (len == 0U)) {
		return -EINVAL;
	}

	if ((ptr_t)addr + len < (ptr_t)addr) {
		/* Overflow */
		return -EINVAL;
	}

	if (vm_mapBelongs(caller, ptr, sizeof(*ptr)) < 0) {
		return -EFAULT;
	}

	(void)proc_lockSet(&coredump_common.lock);

	err = _coredump_currentlyDumped(&crashed);
	if (err == EOK) {
		if (vm_mapBelongs(crashed->process, addr, len) < 0) {
			err = -EINVAL;
		}
		else {
			_coredump_unmapWindow(caller);
			err = _coredump_mapPages(crashed->process, caller, addr, len, &mapped);
		}
	}

	(void)proc_lockClear(&coredump_common.lock);

	if (err == EOK) {
		*ptr = mapped;
	}

	return err;
}


int coredump_done(void)
{
	thread_t *crashed;
	int err;
	spinlock_ctx_t sc;

	(void)proc_lockSet(&coredump_common.lock);

	err = _coredump_currentlyDumped(&crashed);
	if (err == EOK) {
		_coredump_unmapWindow(proc_current()->process);
		coredump_common.current = NULL;

		hal_spinlockSet(&coredump_common.spinlock, &sc);
		coredump_common.actServer = NULL;
		hal_spinlockClear(&coredump_common.spinlock, &sc);
	}

	(void)proc_lockClear(&coredump_common.lock);

	if (err == EOK) {
		coredump_release(crashed);
	}

	return err;
}


void _coredump_init(void)
{
	hal_spinlockCreate(&coredump_common.spinlock, "coredump");
	(void)proc_lockInit(&coredump_common.lock, &proc_lockAttrDefault, "coredump");

	coredump_common.actServer = NULL;
	coredump_common.crashedq = NULL;
	coredump_common.crashedqLen = 0;
	coredump_common.waitq = NULL;
	coredump_common.current = NULL;
	coredump_common.window.vaddr = NULL;
	coredump_common.window.size = 0;
}
