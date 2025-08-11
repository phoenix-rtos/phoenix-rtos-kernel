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

struct {
	spinlock_t spinlock;
	thread_t *toDump;
	process_t *current;
	thread_t *dumperQ;
	oid_t oid;
} coredump_common;


void coredump_enqueue(process_t *process)
{
	thread_t *thread;
	spinlock_ctx_t sc;

	/* No need to protect process.ghosts, its a dying process with last reference here */
	thread = process->ghosts;

	do {
		if (thread->sigpend & (1u << SIGNULL)) {
			/* This thread caused the crash */
			break;
		}
		thread = thread->procnext;
	} while (thread != process->ghosts);

	hal_spinlockSet(&coredump_common.spinlock, &sc);
	LIST_ADD(&coredump_common.toDump, thread);
	proc_threadWakeup(&coredump_common.dumperQ);
	hal_spinlockClear(&coredump_common.spinlock, &sc);
}


int coredump_memEntryList(coredump_memseg *list, size_t n)
{
	map_entry_t *e;
	size_t segCnt = 0;
	size_t i = 0;

	proc_lockSet(&coredump_common.current->mapp->lock);

#ifdef NOMMU
	e = coredump_common.current->entries;
	do {
		if ((e->prot & PROT_READ) && (e->prot & PROT_WRITE)) {
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
		if ((e->prot & PROT_READ) && (e->prot & PROT_WRITE)) {
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

	proc_lockClear(&coredump_common.current->mapp->lock);

	return segCnt;
}


static int coredump_handleMemRead(msg_t *msg, msg_rid_t rid)
{
	const coredump_req *req = (coredump_req *)msg->i.data;
	u32 responsePort = req->mem.responsePort;
	void *startAddr = req->mem.startAddr;
	size_t len = req->mem.size;

	if (vm_mapBelongs(coredump_common.current, req->mem.startAddr, len) != 0) {
		msg->o.err = EINVAL;
		proc_respond(coredump_common.oid.port, msg, rid);
		return EINVAL;
	}
	msg->o.err = EOK;
	proc_respond(coredump_common.oid.port, msg, rid);

	msg_t memMsg;
	memMsg.type = mtWrite;
	memMsg.oid.id = 0;
	memMsg.oid.port = responsePort;
	memMsg.i.size = len;
	memMsg.i.data = startAddr;
	memMsg.o.size = 0;

	/* temporarily set your process to dumped one to use its memory map in messaging */
	proc_current()->process = coredump_common.current;
	int ret = proc_send(responsePort, &memMsg);
	proc_current()->process = NULL;

	if (ret != 0) {
		return ret;
	}
	if (memMsg.o.err != 0) {
		return memMsg.o.err;
	}

	return EOK;
}


static int coredump_handleThreadRead(msg_t *msg, msg_rid_t rid)
{
	const coredump_req *req = (coredump_req *)msg->i.data;
	coredump_thread *threadResp;
	thread_t *thread;

	threadResp = (coredump_thread *)msg->o.data;
	thread = coredump_common.current->ghosts;
	do {
		if (proc_getTid(thread) == req->thread.tid) {
			break;
		}
		thread = thread->procnext;
	} while (thread != coredump_common.current->ghosts);

	if ((thread == NULL) || (proc_getTid(thread) != req->thread.tid)) {
		msg->o.err = ENOENT;
		proc_respond(coredump_common.oid.port, msg, rid);
		return ENOENT;
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
	proc_respond(coredump_common.oid.port, msg, rid);
	return EOK;
}


static int coredump_handleMemListRead(msg_t *msg, msg_rid_t rid)
{
	coredump_memEntryList(msg->o.data, msg->o.size / sizeof(coredump_memseg));
	msg->o.err = EOK;
	proc_respond(coredump_common.oid.port, msg, rid);
	return EOK;
}


static int coredump_handleRelocRead(msg_t *msg, msg_rid_t rid)
{
#ifdef NOMMU

	int i;
	process_t *process = coredump_common.current;

	coredump_reloc *relocs = (coredump_reloc *)msg->o.data;
	for (i = 0; process->reloc[i].pbase != NULL; i++) {
		if ((i + 1) * sizeof(coredump_reloc) >= msg->o.size) {
			break;
		}
		relocs[i].vbase = process->reloc[i].vbase;
		relocs[i].pbase = process->reloc[i].pbase;
	}

	msg->o.err = EOK;
	proc_respond(coredump_common.oid.port, msg, rid);
	return EOK;

#else

	msg->o.err = ENOSYS;
	proc_respond(coredump_common.oid.port, msg, rid);
	return ENOSYS;
#endif
}


int coredump_handleRead(msg_t *msg, msg_rid_t rid)
{
	const coredump_req *req = (coredump_req *)msg->i.data;

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
			break;
	}
	msg->o.err = EINVAL;
	proc_respond(coredump_common.oid.port, msg, rid);
	return EINVAL;
}


int coredump_threadCnt(process_t *process)
{
	thread_t *t = process->ghosts;
	int count = 0;
	if (t == NULL) {
		return 0;
	}
	do {
		count++;
		t = t->procnext;
	} while (t != process->ghosts);

	return count;
}


int coredump_isRunning(int pid)
{
	process_t *p = proc_find(pid);
	if (p == NULL) {
		return 0;
	}
	proc_put(p);
	return 1;
}


void coredump_dump(void)
{
	spinlock_ctx_t scp;
	int srvPid;
	msg_t msg;
	msg_rid_t rid;
	int pathlen;
	char *path;
	thread_t *crashed;

	hal_spinlockSet(&coredump_common.spinlock, &scp);

	while (coredump_common.toDump == NULL) {
		proc_threadWait(&coredump_common.dumperQ, &coredump_common.spinlock, 0, &scp);
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
			proc_respond(coredump_common.oid.port, &msg, rid);
			continue;
		}
	} while (msg.type != mtOpen);

	coredump_general *resp = (coredump_general *)msg.o.data;
	resp->pid = process_getPid(coredump_common.current);
	resp->tid = proc_getTid(crashed);
	resp->signo = hal_cpuGetLastBit((crashed->process->sigpend | crashed->sigpend) & ~(1u << SIGNULL));
	resp->memSegCnt = coredump_memEntryList(NULL, 0);
	resp->threadCnt = coredump_threadCnt(coredump_common.current);
	resp->type = (sizeof(void *) == 8) ? COREDUMP_TYPE_64 : COREDUMP_TYPE_32;
	pathlen = hal_strlen(coredump_common.current->path) + 1;
	path = coredump_common.current->path;
	if (pathlen > sizeof(resp->path)) {
		pathlen = sizeof(resp->path);
		path += pathlen - sizeof(resp->path);
	}
	hal_memcpy(resp->path, path, pathlen);
	srvPid = msg.pid;
	msg.o.err = EOK;

	proc_respond(coredump_common.oid.port, &msg, rid);

	do {
		if (proc_recv(coredump_common.oid.port, &msg, &rid) != 0) {
			continue;
		}
		if (msg.pid != srvPid) {
			msg.o.err = -EBUSY;
			proc_respond(coredump_common.oid.port, &msg, rid);

			if (coredump_isRunning(srvPid)) {
				continue;
			}
			else {
				break;
			}
		}

		if (msg.type == mtRead) {
			coredump_handleRead(&msg, rid);
		}
		else if (msg.type == mtClose) {
			msg.o.err = EOK;
			proc_respond(coredump_common.oid.port, &msg, rid);
		}
		else {
			msg.o.err = -EINVAL;
			proc_respond(coredump_common.oid.port, &msg, rid);
		}
	} while (msg.type != mtClose);

	coredump_common.current->coredump = 0;
	proc_put(coredump_common.current);
	coredump_common.current = NULL;
}


void coredump_msgthr(void *arg)
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

	proc_portCreate(&coredump_common.oid.port);

	proc_threadCreate(NULL, coredump_msgthr, NULL, 4, SIZE_KSTACK, NULL, 0, NULL);
}
