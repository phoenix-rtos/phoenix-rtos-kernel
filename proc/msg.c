/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk, Aleksander Kaminski, Jan Sikorski
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "include/errno.h"
#include "lib/lib.h"
#include "proc.h"


#define FLOOR(x) ((x) & ~(SIZE_PAGE - 1U))
#define CEIL(x)  (((x) + SIZE_PAGE - 1U) & ~(SIZE_PAGE - 1U))


/* clang-format off */
enum { msg_rejected = -1, msg_waiting = 0, msg_received, msg_responded };
/* clang-format on */


static struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
} msg_common;


/* Maps a shadow page into process map dstmap at dst, holding len bytes from src at offs (rest zeroed).
 * The page is anon-backed, so it is refcounted like any other page and stays valid as long as any holder
 * (the receiver window or a borrower, e.g. when the receiver forwards the data) maps it. */
static int msg_mapShadow(vm_map_t *dstmap, void *dst, vm_prot_t prot, vm_flags_t flags, size_t offs, const void *src, size_t len)
{
	page_t *p;
	void *v;

	/* Fill the page while only the kernel sees it, then publish it with its final protection */
	p = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
	if (p == NULL) {
		return -ENOMEM;
	}

	v = vm_mmap(msg_common.kmap, NULL, p, SIZE_PAGE, PROT_READ | PROT_WRITE, msg_common.kernel, -1, MAP_NONE, 0U);
	if (v == NULL) {
		vm_pageFree(p);
		return -ENOMEM;
	}
	/* Don't leak stale page contents to the receiver */
	hal_memset(v, 0, offs);
	hal_memcpy(v + offs, src, len);
	hal_memset(v + offs + len, 0, SIZE_PAGE - offs - len);
	(void)vm_munmap(msg_common.kmap, v, SIZE_PAGE);

	/* Consumes p */
	return vm_mapAnonPage(dstmap, dst, p, prot, flags | MAP_NOINHERIT);
}


/* Copies size bytes between kernel buffer buf and data in map (into buf if out == 0, back to data otherwise).
 * Each source page is pinned only for the duration of its copy, no mapping of it is kept. */
static int msg_copy(vm_map_t *map, void *data, void *buf, size_t size, int out)
{
	map_entry_t e;
	size_t offs, len;
	void *page, *v;

	while (size > 0U) {
		page = (void *)FLOOR((ptr_t)data);
		offs = (size_t)((ptr_t)data - (ptr_t)page);
		len = min(size, SIZE_PAGE - offs);

		v = vm_mapBorrowPage(&e, map, page, (out != 0) ? (PROT_READ | PROT_WRITE) : PROT_READ);
		if (v == NULL) {
			return -EFAULT;
		}

		if (out != 0) {
			hal_memcpy(v + offs, buf, len);
		}
		else {
			hal_memcpy(buf, v + offs, len);
		}

		vm_mapReturnPage(&e);

		data += len;
		buf += len;
		size -= len;
	}

	return EOK;
}


/* Kernel receiver: copy the data into a private kernel buffer instead of mapping the sender pages. Zero-copy
 * windows need refcounted (amap-backed) borrowed entries, which the kernel map can't hold - releasing them
 * calls vm_kfree() with the kmap lock held, which deadlocks when a kmalloc zone gets released */
static void *msg_mapKernel(struct _kmsg_layout_t *ml, void *data, size_t size, vm_map_t *datamap)
{
	ml->kp = vm_pageAlloc(size, PAGE_OWNER_KERNEL | PAGE_KERNEL_HEAP);
	if (ml->kp == NULL) {
		return NULL;
	}

	ml->wsz = (size_t)1U << ml->kp->idx;
	ml->w = vm_mmap(msg_common.kmap, NULL, ml->kp, ml->wsz, PROT_READ | PROT_WRITE, msg_common.kernel, -1, MAP_NONE, 0U);
	if (ml->w == NULL) {
		return NULL;
	}

	ml->src = data;
	ml->srcmap = datamap;

	if (msg_copy(datamap, data, ml->w, size, 0) < 0) {
		return NULL;
	}

	return ml->w;
}


static void *msg_map(int dir, kmsg_t *kmsg, void *data, size_t size, process_t *from, process_t *to)
{
	void *w = NULL, *vaddr;
	size_t boffs, eoffs;
	u8 bone, eone;
	size_t n = 0;
	vm_prot_t prot, sprot;
	vm_map_t *srcmap, *dstmap, *datamap;
	struct _kmsg_layout_t *ml = (dir != 0) ? &kmsg->o : &kmsg->i;
	int err;
	vm_flags_t flags;

	if ((size == 0U) || (data == NULL)) {
		return NULL;
	}

	prot = PROT_READ;

	if (dir != 0) {
		prot |= PROT_WRITE;
	}

	/* Kernel mapping of the source pages - written back on respond only for output */
	sprot = prot;

	if (to != NULL) {
		prot |= PROT_USER;
	}

	boffs = (size_t)(ptr_t)data & (size_t)(SIZE_PAGE - 1U);

	if (FLOOR((ptr_t)data + size) > CEIL((ptr_t)data)) {
		n = (FLOOR((ptr_t)data + size) - CEIL((ptr_t)data)) / SIZE_PAGE;
	}

	if ((boffs != 0U) && (FLOOR((ptr_t)data) == FLOOR((ptr_t)data + size))) {
		/* Data is on one page only and will be copied by boffs handler */
		eoffs = 0U;
	}
	else {
		eoffs = ((size_t)(ptr_t)data + size) & (size_t)(SIZE_PAGE - 1U);
	}

	bone = (boffs != 0U) ? 1U : 0U;
	eone = (eoffs != 0U) ? 1U : 0U;

	srcmap = (from == NULL) ? msg_common.kmap : from->mapp;
	dstmap = (to == NULL) ? msg_common.kmap : to->mapp;

	if ((srcmap == dstmap) && (pmap_belongs(&dstmap->pmap, data) != 0)) {
		return data;
	}

	datamap = (pmap_belongs(&srcmap->pmap, data) != 0) ? srcmap : msg_common.kmap;

	if (dstmap == msg_common.kmap) {
		return msg_mapKernel(ml, data, size, datamap);
	}

	ml->wsz = (n + bone + eone) * SIZE_PAGE;
	w = vm_mapFind(dstmap, NULL, ml->wsz, MAP_NOINHERIT, prot);
	ml->w = w;
	if (w == NULL) {
		return NULL;
	}

	err = vm_mapFlags(datamap, data);
	if (err < 0) {
		return NULL;
	}
	/* Shadow pages are fresh memory - keep only the caching attributes of the source */
	flags = (vm_flags_t)err & (MAP_UNCACHED | MAP_DEVICE);

	if (boffs != 0U) {
		ml->boffs = boffs;

		vaddr = vm_mapBorrowPage(&ml->be, datamap, (void *)FLOOR((ptr_t)data), sprot);
		if (vaddr == NULL) {
			return NULL;
		}

		if (msg_mapShadow(dstmap, w, prot, flags, boffs, vaddr + boffs, (size_t)min(size, SIZE_PAGE - boffs)) < 0) {
			return NULL;
		}
	}

	/* Map pages */
	if (n > 0U) {
		err = vm_mapBorrow(dstmap, w + bone * SIZE_PAGE, datamap, (void *)CEIL((ptr_t)data), n * SIZE_PAGE, prot);
		if (err < 0) {
			return NULL;
		}
	}

	if (eoffs != 0U) {
		ml->eoffs = eoffs;

		vaddr = vm_mapBorrowPage(&ml->ee, datamap, (void *)FLOOR((ptr_t)data + size), sprot);
		if (vaddr == NULL) {
			return NULL;
		}

		if (msg_mapShadow(dstmap, w + (n + bone) * SIZE_PAGE, prot, flags, 0U, vaddr, eoffs) < 0) {
			return NULL;
		}
	}

	return (w + boffs);
}


static void msg_releaseLayout(struct _kmsg_layout_t *ml, vm_map_t *map)
{
	vm_mapReturnPage(&ml->be);
	vm_mapReturnPage(&ml->ee);

	if (ml->kp != NULL) {
		/* Kernel receiver buffer */
		if (ml->w != NULL) {
			(void)vm_munmap(msg_common.kmap, ml->w, ml->wsz);
			ml->w = NULL;
		}
		vm_pageFree(ml->kp);
		ml->kp = NULL;
	}
	else if (ml->w != NULL) {
		/* Shadow pages are anon-backed - unmapping drops the window's reference to them */
		(void)vm_munmap(map, ml->w, ml->wsz);
		ml->w = NULL;
	}

	ml->boffs = 0;
	ml->eoffs = 0;
}


static void msg_release(kmsg_t *kmsg)
{
	process_t *process;
	vm_map_t *map;

	process = proc_current()->process;
	if (process != NULL) {
		map = process->mapp;
	}
	else {
		map = msg_common.kmap;
	}

	msg_releaseLayout(&kmsg->i, map);
	msg_releaseLayout(&kmsg->o, map);
}


static void msg_ipack(kmsg_t *kmsg)
{
	size_t offset;

	if (kmsg->msg.i.data != NULL) {
		switch (kmsg->msg.type) {
			case mtOpen:
			case mtClose:
				offset = sizeof(kmsg->msg.i.openclose);
				break;

			case mtRead:
			case mtWrite:
			case mtTruncate:
				offset = sizeof(kmsg->msg.i.io);
				break;

			case mtCreate:
				offset = sizeof(kmsg->msg.i.create);
				break;

			case mtLookup:
			case mtDestroy:
			case mtGetAttrAll:
				offset = 0;
				break;

			case mtSetAttr:
			case mtGetAttr:
				offset = sizeof(kmsg->msg.i.attr);
				break;

			case mtLink:
			case mtUnlink:
				offset = sizeof(kmsg->msg.i.ln);
				break;

			case mtReaddir:
				offset = sizeof(kmsg->msg.i.readdir);
				break;

			case mtDevCtl:
			default:
				return;
		}

		if (kmsg->msg.i.size > (sizeof(kmsg->msg.i.raw) - offset)) {
			return;
		}

		hal_memcpy(kmsg->msg.i.raw + offset, kmsg->msg.i.data, kmsg->msg.i.size);
		kmsg->msg.i.data = kmsg->msg.i.raw + offset;
	}
}


static int msg_opack(kmsg_t *kmsg)
{
	size_t offset;

	if (kmsg->msg.o.data == NULL) {
		return 0;
	}

	switch (kmsg->msg.type) {
		case mtOpen:
		case mtClose:
		case mtRead:
		case mtWrite:
		case mtTruncate:
		case mtDestroy:
		case mtLink:
		case mtUnlink:
		case mtReaddir:
		case mtGetAttrAll:
			offset = 0;
			break;

		case mtCreate:
			offset = sizeof(kmsg->msg.o.create);
			break;

		case mtSetAttr:
		case mtGetAttr:
			offset = sizeof(kmsg->msg.o.attr);
			break;

		case mtLookup:
			offset = sizeof(kmsg->msg.o.lookup);
			break;

		case mtDevCtl:
		default:
			return 0;
	}

	if (kmsg->msg.o.size > (sizeof(kmsg->msg.o.raw) - offset)) {
		return 0;
	}

	kmsg->msg.o.data = kmsg->msg.o.raw + offset;

	return 1;
}


static int proc_sendEx(u32 port, msg_t *msg, int interruptible)
{
	port_t *p;
	int err = EOK;
	kmsg_t kmsg;
	thread_t *sender;
	spinlock_ctx_t sc;
	int state;

	/* TODO - check if msg pointer belongs to user vm_map */
	if (msg == NULL) {
		return -EINVAL;
	}

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	sender = proc_current();

	hal_memcpy(&kmsg.msg, msg, sizeof(msg_t));
	kmsg.src = sender->process;
	kmsg.threads = NULL;
	kmsg.state = msg_waiting;

	kmsg.msg.pid = (sender->process != NULL) ? process_getPid(sender->process) : 0;
	kmsg.msg.priority = sender->priority;

	msg_ipack(&kmsg);

	hal_spinlockSet(&p->spinlock, &sc);

	if (p->closed != 0) {
		err = -EINVAL;
	}
	else {
		LIST_ADD(&p->kmessages, &kmsg);
		(void)proc_threadWakeup(&p->threads);

		state = kmsg.state;
		while ((state != msg_responded) && (state != msg_rejected)) {
			if (state == msg_received) {
				/* FIXME: due to allocation of kmsg on sender's kstack, this thread cannot be properly killed (or, more precisely,
				 * can't leave this while loop) when message is in msg_received state (between calls to proc_recv and proc_respond).
				 * This uninterruptible wait is a workaround to prevent spinning on proc_threadWaitInterruptible when thread->exit != 0
				 * and blocking a whole system (p->spinlock is held). When thread is running normally, the code behaves just like with
				 * interruptible wait, without spinning on while loop on each interrupt in msg_received state.
				 */
				err = proc_threadWait(&kmsg.threads, &p->spinlock, 0, &sc);
			}
			else if (interruptible != 0) {
				err = proc_threadWaitInterruptible(&kmsg.threads, &p->spinlock, 0, &sc);
			}
			else {
				err = proc_threadWait(&kmsg.threads, &p->spinlock, 0, &sc);
			}

			state = kmsg.state;
			if ((err != EOK) && (state == msg_waiting)) {
				LIST_REMOVE(&p->kmessages, &kmsg);
				break;
			}
		}

		switch (state) {
			case msg_responded:
				err = EOK; /* Don't report EINTR if we got the response already */
				break;
			case msg_rejected:
				err = -EINVAL;
				break;
			default:
				/* No action required */
				break;
		}
	}

	hal_spinlockClear(&p->spinlock, &sc);
	port_put(p, 0);

	if (err == EOK) {
		hal_memcpy(msg->o.raw, kmsg.msg.o.raw, sizeof(msg->o.raw));
		msg->o.err = kmsg.msg.o.err;

		/* If msg.o.data has been packed to msg.o.raw */
		if ((kmsg.msg.o.data >= (void *)kmsg.msg.o.raw) && (kmsg.msg.o.data < (void *)kmsg.msg.o.raw + sizeof(kmsg.msg.o.raw))) {
			hal_memcpy(msg->o.data, kmsg.msg.o.data, msg->o.size);
		}
	}

	return err;
}


int proc_send(u32 port, msg_t *msg)
{
	return proc_sendEx(port, msg, 1);
}


int proc_sendUninterruptible(u32 port, msg_t *msg)
{
	return proc_sendEx(port, msg, 0);
}


int proc_recv(u32 port, msg_t *msg, msg_rid_t *rid)
{
	port_t *p;
	kmsg_t *kmsg;
	int ipacked = 0, opacked = 0, err = EOK;
	spinlock_ctx_t sc;

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	hal_spinlockSet(&p->spinlock, &sc);

	while ((p->kmessages == NULL) && (p->closed == 0) && (err != -EINTR)) {
		err = proc_threadWaitInterruptible(&p->threads, &p->spinlock, 0, &sc);
	}

	kmsg = p->kmessages;

	if (p->closed != 0) {
		/* Port is being removed */
		if (kmsg != NULL) {
			kmsg->state = msg_rejected;
			LIST_REMOVE(&p->kmessages, kmsg);
			(void)proc_threadWakeup(&kmsg->threads);
		}

		err = -EINVAL;
	}
	else {
		if (err == EOK) {
			LIST_REMOVE(&p->kmessages, kmsg);
			kmsg->state = msg_received;
		}
	}
	hal_spinlockClear(&p->spinlock, &sc);

	if (err != EOK) {
		port_put(p, 0);
		return err;
	}

	hal_memset(&kmsg->i, 0, sizeof(kmsg->i));
	hal_memset(&kmsg->o, 0, sizeof(kmsg->o));

	if ((kmsg->msg.i.data >= (void *)kmsg->msg.i.raw) && (kmsg->msg.i.data < (void *)kmsg->msg.i.raw + sizeof(kmsg->msg.i.raw))) {
		ipacked = 1;
	}

	/* Map data in receiver space */
	/* Don't map if msg is packed */
	if (ipacked == 0) {
		kmsg->msg.i.data = msg_map(0, kmsg, (void *)(ptr_t)kmsg->msg.i.data, kmsg->msg.i.size, kmsg->src, proc_current()->process);
	}

	opacked = msg_opack(kmsg);
	if (opacked == 0) {
		kmsg->msg.o.data = msg_map(1, kmsg, kmsg->msg.o.data, kmsg->msg.o.size, kmsg->src, proc_current()->process);
	}

	if (((kmsg->msg.i.size != 0U) && (kmsg->msg.i.data == NULL)) ||
			((kmsg->msg.o.size != 0U) && (kmsg->msg.o.data == NULL)) ||
			(proc_portRidAlloc(p, kmsg) < 0)) {
		msg_release(kmsg);

		hal_spinlockSet(&p->spinlock, &sc);
		kmsg->state = msg_rejected;
		(void)proc_threadWakeup(&kmsg->threads);
		hal_spinlockClear(&p->spinlock, &sc);

		port_put(p, 0);

		return -ENOMEM;
	}

	*rid = lib_idtreeId(&kmsg->idlinkage);

	hal_memcpy(msg, &kmsg->msg, sizeof(*msg));

	if (ipacked != 0) {
		msg->i.data = msg->i.raw + (kmsg->msg.i.data - (void *)kmsg->msg.i.raw);
	}

	if (opacked != 0) {
		msg->o.data = msg->o.raw + (kmsg->msg.o.data - (void *)kmsg->msg.o.raw);
	}

	port_put(p, 0);

	return EOK;
}


int proc_respond(u32 port, msg_t *msg, msg_rid_t rid)
{
	port_t *p;
	kmsg_t *kmsg;
	spinlock_ctx_t sc;

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	kmsg = proc_portRidGet(p, rid);
	if (kmsg == NULL) {
		return -ENOENT;
	}

	/* Copy output shadow pages back (input is mapped read-only in the receiver) */
	if (kmsg->o.be.map != NULL) {
		hal_memcpy(kmsg->o.be.vaddr + kmsg->o.boffs, kmsg->o.w + kmsg->o.boffs, (size_t)min(SIZE_PAGE - kmsg->o.boffs, kmsg->msg.o.size));
	}

	if (kmsg->o.ee.map != NULL) {
		hal_memcpy(kmsg->o.ee.vaddr, kmsg->o.w + kmsg->o.boffs + kmsg->msg.o.size - kmsg->o.eoffs, (size_t)kmsg->o.eoffs);
	}

	/* Kernel receiver - copy the whole output buffer back */
	if ((kmsg->o.kp != NULL) && (kmsg->o.w != NULL)) {
		(void)msg_copy(kmsg->o.srcmap, kmsg->o.src, kmsg->o.w, kmsg->msg.o.size, 1);
	}

	msg_release(kmsg);

	hal_memcpy(kmsg->msg.o.raw, msg->o.raw, sizeof(msg->o.raw));
	kmsg->msg.o.err = msg->o.err;

	hal_spinlockSet(&p->spinlock, &sc);
	kmsg->state = msg_responded;
	kmsg->src = proc_current()->process;
	(void)proc_threadWakeup(&kmsg->threads);
	hal_spinlockClear(&p->spinlock, &sc);
	(void)hal_cpuReschedule(NULL, NULL);

	port_put(p, 0);

	return EOK;
}


void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
	msg_common.kmap = kmap;
	msg_common.kernel = kernel;
}
