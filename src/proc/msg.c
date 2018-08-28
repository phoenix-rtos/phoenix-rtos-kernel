/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Messages
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk, Aleksander Kaminski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "../../include/errno.h"
#include "../lib/lib.h"
#include "proc.h"


#define FLOOR(x)    ((x) & ~(SIZE_PAGE - 1))
#define CEIL(x)     (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
} msg_common;


static void *msg_map(int dir, kmsg_t *kmsg, void *data, size_t size, process_t *from, process_t *to)
{
	void *w = NULL, *vaddr;
	u64 boffs, eoffs;
	unsigned int n = 0, i, attr, prot;
	page_t *ep = NULL, *nep = NULL, *bp = NULL, *nbp = NULL, *p;
	vm_map_t *srcmap, *dstmap;
	struct _kmsg_layout_t *ml = dir ? &kmsg->o : &kmsg->i;

	if ((size == 0) || (data == NULL))
		return EOK;

	attr = PGHD_PRESENT;
	prot = PROT_READ;

	if (dir) {
		attr |= PGHD_WRITE;
		prot |= PROT_WRITE;
	}

	if (to != NULL) {
		attr |= PGHD_USER;
		prot |= PROT_USER;
	}

	boffs = (unsigned long)data & (SIZE_PAGE - 1);

	if (FLOOR((unsigned long)data + size) > CEIL((unsigned long)data))
		n = (FLOOR((unsigned long)data + size) - CEIL((unsigned long)data)) / SIZE_PAGE;

	if (boffs && (FLOOR((unsigned long)data) == FLOOR((unsigned long)data + size)))
		/* Data is on one page only and will be copied by boffs handler */
		eoffs = 0;
	else
		eoffs = ((unsigned long)data + size) & (SIZE_PAGE - 1);

	srcmap = (from == NULL) ? msg_common.kmap : from->mapp;
	dstmap = (to == NULL) ? msg_common.kmap : to->mapp;

	if (srcmap == dstmap && pmap_belongs(&dstmap->pmap, data))
		return data;

	if ((ml->w = w = vm_mapFind(dstmap, (void *)0, (!!boffs + !!eoffs + n) * SIZE_PAGE, MAP_NOINHERIT, prot)) == NULL)
		return NULL;

	if (boffs > 0) {
		ml->boffs = boffs;
		bp = _page_get(pmap_resolve(&srcmap->pmap, data));

		if ((ml->bp = nbp = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP)) == NULL)
			return NULL;

		if ((ml->bvaddr = vaddr = vm_mmap(msg_common.kmap, (void *)0, bp, SIZE_PAGE, PROT_READ | PROT_WRITE, msg_common.kernel, -1, MAP_NONE)) == NULL)
			return NULL;

		/* Map new page into destination address space */
		if (page_map(&dstmap->pmap, w, nbp->addr, (attr | PGHD_WRITE) & ~PGHD_USER) < 0)
			return NULL;

		hal_memcpy(w + boffs, vaddr + boffs, min(size, SIZE_PAGE - boffs));

		if (page_map(&dstmap->pmap, w, nbp->addr, attr) < 0)
			return NULL;
	}

	/* Map pages */
	vaddr = (void *)CEIL((unsigned long)data);

	for (i = 0; i < n; i++, vaddr += SIZE_PAGE) {
		p = _page_get(pmap_resolve(&srcmap->pmap, vaddr));
		if (page_map(&dstmap->pmap, w + (i + !!boffs) * SIZE_PAGE, p->addr, attr) < 0)
			return NULL;
	}

// lib_printf("w=%p bp->addr=%p %p\n", w, nbp->addr, vaddr);

	if (eoffs) {
		ml->eoffs = eoffs;
		vaddr = (void *)FLOOR((unsigned long)data + size);
		ep = _page_get(pmap_resolve(&srcmap->pmap, vaddr));

		if (!boffs || (eoffs >= boffs)) {
			if ((ml->ep = nep = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP)) == NULL)
				return NULL;
		}
		else {
			nep = nbp;
		}

		if ((ml->evaddr = vaddr = vm_mmap(msg_common.kmap, (void *)0, ep, SIZE_PAGE, PROT_READ | PROT_WRITE, msg_common.kernel, -1, MAP_NONE)) == NULL)
			return NULL;

		/* Map new page into destination address space */
		if (page_map(&dstmap->pmap, w + (n + !!boffs) * SIZE_PAGE, nep->addr, (attr | PGHD_WRITE) & ~PGHD_USER) < 0)
			return NULL;

		hal_memcpy(w + (n + !!boffs) * SIZE_PAGE, vaddr, eoffs);

		if (page_map(&dstmap->pmap, w + (n + !!boffs) * SIZE_PAGE, nep->addr, attr) < 0)
			return NULL;
	}

	return (w + boffs);
}


static void msg_release(kmsg_t *kmsg)
{
	process_t *process;

	if (kmsg->i.bp != NULL) {
		vm_pageFree(kmsg->i.bp);
		vm_munmap(msg_common.kmap, kmsg->i.bvaddr, SIZE_PAGE);
		kmsg->i.bp = NULL;
	}

	if (kmsg->i.eoffs) {
		if (kmsg->i.ep != NULL)
			vm_pageFree(kmsg->i.ep);
		vm_munmap(msg_common.kmap, kmsg->i.evaddr, SIZE_PAGE);
		kmsg->i.eoffs = 0;
		kmsg->i.ep = NULL;
	}

	if (kmsg->i.w != NULL) {
		if ((process = proc_current()->process) != NULL)
			vm_munmap(process->mapp, kmsg->i.w, CEIL((unsigned long)kmsg->msg.i.data + kmsg->msg.i.size) - FLOOR((unsigned long)kmsg->msg.i.data));
		kmsg->i.w = NULL;
	}

	if (kmsg->o.bp != NULL) {
		vm_pageFree(kmsg->o.bp);
		vm_munmap(msg_common.kmap, kmsg->o.bvaddr, SIZE_PAGE);
		kmsg->o.bp = NULL;
	}

	if (kmsg->o.eoffs) {
		if (kmsg->o.ep != NULL)
			vm_pageFree(kmsg->o.ep);
		vm_munmap(msg_common.kmap, kmsg->o.evaddr, SIZE_PAGE);
		kmsg->o.eoffs = 0;
		kmsg->o.ep = NULL;
	}

	if (kmsg->o.w != NULL) {
		if ((process = proc_current()->process) != NULL)
			vm_munmap(process->mapp, kmsg->o.w, CEIL((unsigned long)kmsg->msg.o.data + kmsg->msg.o.size) - FLOOR((unsigned long)kmsg->msg.o.data));
		kmsg->o.w = NULL;
	}
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

			case mtDestroy:
				offset = sizeof(kmsg->msg.i.destroy);
				break;

			case mtSetAttr:
			case mtGetAttr:
				offset = sizeof(kmsg->msg.i.attr);
				break;

			case mtLookup:
				offset = sizeof(kmsg->msg.i.lookup);
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

		if (kmsg->msg.i.size > sizeof(kmsg->msg.i.raw) - offset)
			return;

		hal_memcpy(kmsg->msg.i.raw + offset, kmsg->msg.i.data, kmsg->msg.i.size);
		kmsg->msg.i.data = kmsg->msg.i.raw + offset;
	}
}


static int msg_opack(kmsg_t *kmsg)
{
	size_t offset;

	if (kmsg->msg.o.data == NULL)
		return 0;

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
			offset = sizeof(kmsg->msg.o.io);
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

	if (kmsg->msg.o.size > sizeof(kmsg->msg.o.raw) - offset)
		return 0;

	kmsg->msg.o.data = kmsg->msg.o.raw + offset;

	return 1;
}


int proc_send(u32 port, msg_t *msg)
{
	port_t *p;
	int state = msQueued;
	int err = EOK;
	kmsg_t *kmsg;

	if ((p = proc_portGet(port)) == NULL)
		return -EINVAL;

	if ((kmsg = vm_kmalloc(sizeof(kmsg_t))) == NULL) {
		port_put(p, 0);
		return -ENOMEM;
	}

	hal_memcpy(&kmsg->msg, msg, sizeof(msg_t));
	kmsg->src = proc_current()->process;
	kmsg->threads = NULL;
	kmsg->state = msQueued;

	msg_ipack(kmsg);

	hal_spinlockSet(&p->spinlock);

	if (p->closed) {
		err = -EINVAL;
	}
	else {
		LIST_ADD(&p->kmessages, kmsg);
		proc_threadWakeup(&p->threads);

		proc_threadUnprotect();
		while ((state = kmsg->state) == msQueued || state == msProcessing) {
			if ((err = proc_threadWait(&kmsg->threads, &p->spinlock, 0)) == -EINTR) {
				/* Thread was unprotected, abort and leave */
				switch (kmsg->state) {
				case msAborted:
					 /* aborted already, mark as responded to free it on the way out */
					kmsg->state = msResponded;
					break;
				case msQueued:
					LIST_REMOVE(&p->kmessages, kmsg);
					break;
				case msProcessing:
					LIST_REMOVE(&p->received, kmsg);
					break;
				case msResponded:
					/* Let's pretend that the signal came just _after_ we woke up */
					err = EOK;
					break;
				}
				kmsg->state = msAborted;
				break;
			}
		}
		proc_threadProtect();
	}

	hal_spinlockClear(&p->spinlock);
	port_put(p, 0);

	hal_memcpy(msg->o.raw, kmsg->msg.o.raw, sizeof(msg->o.raw));

	/* If msg.o.data has been packed to msg.o.raw */
	if ((kmsg->msg.o.data > (void *)kmsg->msg.o.raw) && (kmsg->msg.o.data < (void *)kmsg->msg.o.raw + sizeof(kmsg->msg.o.raw)))
		hal_memcpy(msg->o.data, kmsg->msg.o.data, msg->o.size);

	if (kmsg->state == msResponded)
		vm_kfree(kmsg);

	return state < 0 ? -EINVAL : err;
}


int proc_recv(u32 port, msg_t *msg, unsigned int *rid)
{
	port_t *p;
	kmsg_t *kmsg;
	int ipacked = 0, opacked = 0, closed;

	if ((p = proc_portGet(port)) == NULL)
		return -EINVAL;

	proc_threadUnprotect();
	hal_spinlockSet(&p->spinlock);

	while (p->kmessages == NULL && !p->closed)
		proc_threadWait(&p->threads, &p->spinlock, 0);

	kmsg = p->kmessages;

	if (p->closed) {
		/* Port is being removed */
		if (kmsg != NULL) {
			kmsg->state = msAborted;
			LIST_REMOVE(&p->kmessages, kmsg);
			proc_threadWakeup(&kmsg->threads);
		}

		hal_spinlockClear(&p->spinlock);
		proc_threadProtect();
		port_put(p, 0);
		return -EINVAL;
	}

	kmsg->state = msProcessing;
	LIST_REMOVE(&p->kmessages, kmsg);
	LIST_ADD(&p->received, kmsg);
	hal_spinlockClear(&p->spinlock);
	proc_threadProtect();

	/* (MOD) */
	(*rid) = (unsigned long)(kmsg);

	kmsg->i.bvaddr = NULL;
	kmsg->i.boffs = 0;
	kmsg->i.w = NULL;
	kmsg->i.bp = NULL;
	kmsg->i.evaddr = NULL;
	kmsg->i.eoffs = 0;
	kmsg->i.ep = NULL;

	kmsg->o.bvaddr = NULL;
	kmsg->o.boffs = 0;
	kmsg->o.w = NULL;
	kmsg->o.bp = NULL;
	kmsg->o.evaddr = NULL;
	kmsg->o.eoffs = 0;
	kmsg->o.ep = NULL;

	if ((kmsg->msg.i.data > (void *)kmsg->msg.i.raw) && (kmsg->msg.i.data < (void *)kmsg->msg.i.raw + sizeof(kmsg->msg.i.raw)))
		ipacked = 1;

	/* Map data in receiver space */
	/* Don't map if msg is packed */
	if (!ipacked)
		kmsg->msg.i.data = msg_map(0, kmsg, kmsg->msg.i.data, kmsg->msg.i.size, kmsg->src, proc_current()->process);

	if (!(opacked = msg_opack(kmsg)))
		kmsg->msg.o.data = msg_map(1, kmsg, kmsg->msg.o.data, kmsg->msg.o.size, kmsg->src, proc_current()->process);

	if ((kmsg->msg.i.size && kmsg->msg.i.data == NULL) ||
		(kmsg->msg.o.size && kmsg->msg.o.data == NULL) ||
		p->closed) {
		closed = p->closed;
		msg_release(kmsg);

		hal_spinlockSet(&p->spinlock);
		LIST_REMOVE(&p->received, kmsg);
		kmsg->state = msAborted;
		proc_threadWakeup(&kmsg->threads);
		hal_spinlockClear(&p->spinlock);

		port_put(p, 0);

		return closed ? -EINVAL : -ENOMEM;
	}

	hal_memcpy(msg, &kmsg->msg, sizeof(*msg));

	if (ipacked)
		msg->i.data = msg->i.raw + (kmsg->msg.i.data - (void *)kmsg->msg.i.raw);

	if (opacked)
		msg->o.data = msg->o.raw + (kmsg->msg.o.data - (void *)kmsg->msg.o.raw);

	port_put(p, 0);
	return EOK;
}


int proc_respond(u32 port, msg_t *msg, unsigned int rid)
{
	port_t *p;
	size_t s = 0;
	kmsg_t *kmsg = (kmsg_t *)(unsigned long)rid;

	if ((p = proc_portGet(port)) == NULL)
		return -EINVAL;

	if (kmsg->state == msAborted) {
		msg_release(kmsg);
		port_put(p, 0);
		return -EINVAL;
	}

	/* Copy shadow pages */
	if (kmsg->i.bp != NULL)
		hal_memcpy(kmsg->i.bvaddr + kmsg->i.boffs, kmsg->i.w + kmsg->i.boffs, min(SIZE_PAGE - kmsg->i.boffs, kmsg->msg.i.size));

	if (kmsg->i.eoffs)
		hal_memcpy(kmsg->i.evaddr, kmsg->i.w + kmsg->i.boffs + kmsg->msg.i.size - kmsg->i.eoffs, kmsg->i.eoffs);

	if (kmsg->o.bp != NULL)
		hal_memcpy(kmsg->o.bvaddr + kmsg->o.boffs, kmsg->o.w + kmsg->o.boffs, min(SIZE_PAGE - kmsg->o.boffs, kmsg->msg.o.size));

	if (kmsg->o.eoffs)
		hal_memcpy(kmsg->o.evaddr, kmsg->o.w + kmsg->o.boffs + kmsg->msg.o.size - kmsg->o.eoffs, kmsg->o.eoffs);

	msg_release(kmsg);

	hal_memcpy(kmsg->msg.o.raw, msg->o.raw, sizeof(msg->o.raw));

	hal_spinlockSet(&p->spinlock);
	kmsg->state = msResponded;
	kmsg->src = proc_current()->process;
	LIST_REMOVE(&p->received, kmsg);
	proc_threadWakeup(&kmsg->threads);
	hal_spinlockClear(&p->spinlock);
	port_put(p, 0);

	return s;
}


int proc_msgStatus(unsigned int rid)
{
	kmsg_t *kmsg = (kmsg_t *)(unsigned long)rid;
	return kmsg->state;
}


void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
	msg_common.kmap = kmap;
	msg_common.kernel = kernel;
}
