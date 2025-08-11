/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Coredump server communication
 *
 * Copyright 2025 Phoenix Systems
 * Author: Jakub Klimek
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "coredump.h"

#include "proc/threads.h"
#include "proc/ports.h"
#include "include/coredump.h"

static struct {
	spinlock_t spinlock;
	thread_t *toDump;
	process_t *current;
	thread_t *dumperQ;
	oid_t oid;
} coredump_common;


int coredump_enqueue(process_t *process)
{
	thread_t *thread, *ghost;
	spinlock_ctx_t sc;

	/* No need to protect process.ghosts, we have the last reference to a dying process */
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
		if ((thread->sigpend & (1U << SIGNULL)) != 0U) {
			break;
		}
		thread = thread->procnext;
	} while (thread != process->ghosts);

	if ((thread->sigpend & (1U << SIGNULL)) == 0U) {
		return -ENOENT;
	}

	hal_spinlockSet(&coredump_common.spinlock, &sc);
	LIST_ADD(&coredump_common.toDump, thread);
	(void)proc_threadWakeup(&coredump_common.dumperQ);
	hal_spinlockClear(&coredump_common.spinlock, &sc);

	return EOK;
}


static size_t coredump_memEntryList(coredump_memseg_t *list, size_t n)
{
	map_entry_t *e;
	size_t segCnt = 0;
	size_t i = 0;

	(void)proc_lockSet(&coredump_common.current->mapp->lock);

#ifdef NOMMU

	e = coredump_common.current->entries;
	do {
		if (((e->prot & PROT_READ) != 0) && ((e->prot & PROT_WRITE) != 0)) {
			segCnt++;
			if (i < n) {
				list[i].startAddr = e->vaddr;
				list[i].endAddr = e->vaddr + e->size;
				i++;
			}
		}
		e = e->next;
	} while (e != coredump_common.current->entries);

#else

	e = lib_treeof(map_entry_t, linkage, lib_rbMinimum(coredump_common.current->mapp->tree.root));
	while (e != NULL) {
		if (((e->prot & PROT_READ) != 0U) && ((e->prot & PROT_WRITE) != 0U)) {
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

	(void)proc_lockClear(&coredump_common.current->mapp->lock);

	return segCnt;
}


static void coredump_handleMemRead(msg_t *msg, msg_rid_t rid)
{
	const coredump_req_t *req = (const coredump_req_t *)msg->i.data;
	u32 responsePort = req->mem.responsePort;
	void *startAddr = req->mem.startAddr;
	size_t len = req->mem.size;
	msg_t memMsg;

	if (vm_mapBelongs(coredump_common.current, req->mem.startAddr, len) != 0) {
		msg->o.err = -EINVAL;
		(void)proc_respond(coredump_common.oid.port, msg, rid);
		return;
	}
	msg->o.err = EOK;
	(void)proc_respond(coredump_common.oid.port, msg, rid);

	memMsg.type = mtWrite;
	memMsg.oid.id = 0;
	memMsg.oid.port = responsePort;
	memMsg.i.size = len;
	memMsg.i.data = startAddr;
	memMsg.o.size = 0;

	(void)proc_sendFromMap(responsePort, &memMsg, coredump_common.current->mapp);
}


static void coredump_handleThreadRead(msg_t *msg, msg_rid_t rid)
{
	const coredump_req_t *req = (const coredump_req_t *)msg->i.data;
	coredump_thread_t *threadResp;
	thread_t *thread;

	threadResp = (coredump_thread_t *)msg->o.data;
	thread = coredump_common.current->ghosts;
	if (thread != NULL) {
		do {
			if (proc_getTid(thread) == req->thread.tid) {
				break;
			}
			thread = thread->procnext;
		} while (thread != coredump_common.current->ghosts);
	}

	if ((thread == NULL) || (proc_getTid(thread) != req->thread.tid)) {
		msg->o.err = -ENOENT;
		(void)proc_respond(coredump_common.oid.port, msg, rid);
	}
	threadResp->stackAddr = thread->ustack;
	threadResp->tid = proc_getTid(thread);
	threadResp->nextTid = proc_getTid(thread->procnext);

	/* Copy userspace context */
	if (hal_cpuSupervisorMode(thread->context) == 0) {
		hal_memcpy(threadResp->context, thread->context, sizeof(cpu_context_t));
	}
	else {
		hal_memcpy(threadResp->context, (char *)thread->kstack + thread->kstacksz - sizeof(cpu_context_t), sizeof(cpu_context_t));
	}

	msg->o.err = EOK;
	(void)proc_respond(coredump_common.oid.port, msg, rid);
}


static void coredump_handleMemListRead(msg_t *msg, msg_rid_t rid)
{
	(void)coredump_memEntryList(msg->o.data, msg->o.size / sizeof(coredump_memseg_t));
	msg->o.err = EOK;
	(void)proc_respond(coredump_common.oid.port, msg, rid);
}


static void coredump_handleRelocRead(msg_t *msg, msg_rid_t rid)
{
#ifdef NOMMU

	size_t i;
	process_t *process = coredump_common.current;

	coredump_reloc_t *relocs = (coredump_reloc_t *)msg->o.data;
	for (i = 0; i < process->relocsz && i < msg->o.size / sizeof(coredump_reloc_t); i++) {
		relocs[i].vbase = process->reloc[i].vbase;
		relocs[i].pbase = process->reloc[i].pbase;
	}
	hal_memset((char*)msg->o.data + i * sizeof(coredump_reloc_t), 0, msg->o.size - i * sizeof(coredump_reloc_t));

	msg->o.err = EOK;
	(void)proc_respond(coredump_common.oid.port, msg, rid);

#else

	msg->o.err = -ENOSYS;
	(void)proc_respond(coredump_common.oid.port, msg, rid);

#endif
}


static void coredump_handleRead(msg_t *msg, msg_rid_t rid)
{
	const coredump_req_t *req = (const coredump_req_t *)msg->i.data;

	switch (req->type) {
		case COREDUMP_REQ_MEM:
			return coredump_handleMemRead(msg, rid);

		case COREDUMP_REQ_THREAD:
			return coredump_handleThreadRead(msg, rid);

		case COREDUMP_REQ_MEMLIST:
			return coredump_handleMemListRead(msg, rid);

		case COREDUMP_REQ_RELOC:
			return coredump_handleRelocRead(msg, rid);

		default:
			msg->o.err = -EINVAL;
			(void)proc_respond(coredump_common.oid.port, msg, rid);
			break;
	}
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


static int coredump_isRunning(int pid)
{
	process_t *p = proc_find(pid);
	if (p == NULL) {
		return 0;
	}
	(void)proc_put(p);
	return 1;
}


static void coredump_dump(void)
{
	spinlock_ctx_t scp;
	int srvPid;
	msg_t msg;
	msg_rid_t rid;
	size_t pathlen;
	char *path;
	thread_t *crashed;
	coredump_general_t *resp;

	hal_spinlockSet(&coredump_common.spinlock, &scp);

	while (coredump_common.toDump == NULL) {
		(void)proc_threadWait(&coredump_common.dumperQ, &coredump_common.spinlock, 0, &scp);
	}
	crashed = coredump_common.toDump;
	coredump_common.current = crashed->process;
	LIST_REMOVE(&coredump_common.toDump, crashed);

	hal_spinlockClear(&coredump_common.spinlock, &scp);

	do {
		if (proc_recv(coredump_common.oid.port, &msg, &rid) != 0) {
			continue;
		}
		if (msg.type != mtOpen) {
			msg.o.err = -EINVAL;
			(void)proc_respond(coredump_common.oid.port, &msg, rid);
			continue;
		}
	} while (msg.type != mtOpen);

	resp = (coredump_general_t *)msg.o.data;
	resp->pid = process_getPid(coredump_common.current);
	resp->tid = proc_getTid(crashed);
	resp->signo = hal_cpuGetLastBit((crashed->process->sigpend | crashed->sigpend) & ~(1U << SIGNULL));
	resp->memSegCnt = coredump_memEntryList(NULL, 0);
	resp->threadCnt = coredump_threadCnt(coredump_common.current);
	resp->type = (sizeof(void *) == 8U) ? COREDUMP_TYPE_64 : COREDUMP_TYPE_32;
	pathlen = hal_strlen(coredump_common.current->path) + 1U;
	path = coredump_common.current->path;
	if (pathlen > sizeof(resp->path)) {
		pathlen = sizeof(resp->path);
		path += pathlen - sizeof(resp->path);
	}
	hal_memcpy(resp->path, path, pathlen);
	srvPid = msg.pid;
	msg.o.err = EOK;

	(void)proc_respond(coredump_common.oid.port, &msg, rid);

	do {
		if (proc_recv(coredump_common.oid.port, &msg, &rid) != 0) {
			continue;
		}
		if (msg.pid != srvPid) {
			msg.o.err = -EBUSY;
			(void)proc_respond(coredump_common.oid.port, &msg, rid);

			if (coredump_isRunning(srvPid) != 0) {
				continue;
			}
			else {
				break;
			}
		}

		switch (msg.type) {
			case mtRead:
				coredump_handleRead(&msg, rid);
				break;
			case mtClose:
				msg.o.err = EOK;
				(void)proc_respond(coredump_common.oid.port, &msg, rid);
				break;
			default:
				msg.o.err = -EINVAL;
				(void)proc_respond(coredump_common.oid.port, &msg, rid);
				break;
		}
	} while (msg.type != mtClose);

	coredump_common.current->coredump = 0;
	(void)proc_put(coredump_common.current);
	coredump_common.current = NULL;
}


static void coredump_msgthr(void *arg)
{
	for (;;) {
		coredump_dump();
	}
}


void _coredump_start(void)
{
	hal_spinlockCreate(&coredump_common.spinlock, "coredump");
	coredump_common.current = NULL;
	coredump_common.toDump = NULL;
	coredump_common.dumperQ = NULL;

	(void)proc_portCreate(&coredump_common.oid.port);

	(void)proc_threadCreate(NULL, coredump_msgthr, NULL, 4, SIZE_KSTACK, NULL, 0, NULL);
}
