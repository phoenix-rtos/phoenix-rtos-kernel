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
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "include/errno.h"
#include "lib/lib.h"
#include "proc.h"
#include "hal/riscv64/riscv64.h"

#define FLOOR(x) ((x) & ~(SIZE_PAGE - 1))
#define CEIL(x)  (((x) + SIZE_PAGE - 1) & ~(SIZE_PAGE - 1))


/* clang-format off */
enum { msg_rejected = -1, msg_waiting = 0, msg_received, msg_responded };
/* clang-format on */


struct {
	vm_map_t *kmap;
	vm_object_t *kernel;
} msg_common;


typedef struct {
	u64 time;
	size_t size;
	ptr_t alignment;
} times_t;

times_t sendTimes[1000];
times_t recvTimes[1000];
times_t respondTimes[1000];

size_t nrespond;
size_t nsend;
size_t nrecv;

u8 buffer[500 * SIZE_PAGE];


void msg_dumpTimes(void)
{
	size_t i;

	lib_printf("Send times: (%zu)\n", nsend);
	lib_printf("Time,Size,Alignment\n");
	for (i = 0; i < nsend; i++) {
		lib_printf("%lu,%zu,%lu\n", sendTimes[i].time, sendTimes[i].size, sendTimes[i].alignment);
	}

	lib_printf("\nRecv times: (%zu)\n", nrecv);
	lib_printf("Time,Size,Alignment\n");
	for (i = 0; i < nrecv; i++) {
		lib_printf("%lu,%zu,%lu\n", recvTimes[i].time, recvTimes[i].size, recvTimes[i].alignment);
	}

	lib_printf("\nRespond times: (%zu)\n", nrespond);
	lib_printf("Time,Size,Alignment\n");
	for (i = 0; i < nrespond; i++) {
		lib_printf("%lu,%zu,%lu\n", respondTimes[i].time, respondTimes[i].size, respondTimes[i].alignment);
	}
}


static void *msg_map(int dir, kmsg_t *kmsg, void *data, size_t size, process_t *from, process_t *to)
{
	return NULL;
	void *w = NULL, *vaddr;
	u64 boffs, eoffs;
	unsigned int n = 0, i, attr, prot;
	page_t *nep = NULL, *nbp = NULL;
	vm_map_t *srcmap, *dstmap;
	struct _kmsg_layout_t *ml = dir ? &kmsg->o : &kmsg->i;
	int flags;
	addr_t bpa, pa, epa;

	if ((size == 0) || (data == NULL)) {
		return NULL;
	}

	attr = PGHD_PRESENT;
	prot = PROT_READ;

	if (dir != 0) {
		attr |= PGHD_WRITE;
		prot |= PROT_WRITE;
	}

	if (to != NULL) {
		attr |= PGHD_USER;
		prot |= PROT_USER;
	}

	boffs = (ptr_t)data & (SIZE_PAGE - 1);

	if (FLOOR((ptr_t)data + size) > CEIL((ptr_t)data)) {
		n = (FLOOR((ptr_t)data + size) - CEIL((ptr_t)data)) / SIZE_PAGE;
	}

	if ((boffs != 0) && (FLOOR((ptr_t)data) == FLOOR((ptr_t)data + size))) {
		/* Data is on one page only and will be copied by boffs handler */
		eoffs = 0;
	}
	else {
		eoffs = ((ptr_t)data + size) & (SIZE_PAGE - 1);
	}

	srcmap = (from == NULL) ? msg_common.kmap : from->mapp;
	dstmap = (to == NULL) ? msg_common.kmap : to->mapp;

	if ((srcmap == dstmap) && (pmap_belongs(&dstmap->pmap, data) != 0)) {
		return data;
	}

	w = vm_mapFind(dstmap, NULL, (((boffs != 0) ? 1 : 0) + ((eoffs != 0) ? 1 : 0) + n) * SIZE_PAGE, MAP_NOINHERIT, prot);
	ml->w = w;
	if (w == NULL) {
		return NULL;
	}

	if (pmap_belongs(&srcmap->pmap, data) != 0) {
		flags = vm_mapFlags(srcmap, data);
	}
	else {
		flags = vm_mapFlags(msg_common.kmap, data);
	}

	if (flags < 0) {
		return NULL;
	}

	attr |= vm_flagsToAttr(flags);

	if (boffs > 0) {
		ml->boffs = boffs;
		bpa = pmap_resolve(&srcmap->pmap, data) & ~(SIZE_PAGE - 1);

		nbp = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
		ml->bp = nbp;
		if (nbp == NULL) {
			return NULL;
		}

		vaddr = vm_mmap(msg_common.kmap, NULL, NULL, SIZE_PAGE, PROT_READ | PROT_WRITE, VM_OBJ_PHYSMEM, bpa, flags);
		ml->bvaddr = vaddr;
		if (vaddr == NULL) {
			return NULL;
		}

		/* Map new page into destination address space */
		if (page_map(&dstmap->pmap, w, nbp->addr, (attr | PGHD_WRITE) & ~PGHD_USER) < 0) {
			return NULL;
		}

		hal_memcpy(w + boffs, vaddr + boffs, min(size, SIZE_PAGE - boffs));

		if (page_map(&dstmap->pmap, w, nbp->addr, attr) < 0) {
			return NULL;
		}
	}

	/* Map pages */
	vaddr = (void *)CEIL((ptr_t)data);

	for (i = 0; i < n; i++, vaddr += SIZE_PAGE) {
		pa = pmap_resolve(&srcmap->pmap, vaddr) & ~(SIZE_PAGE - 1);
		if (page_map(&dstmap->pmap, w + (i + ((boffs != 0) ? 1 : 0)) * SIZE_PAGE, pa, attr) < 0) {
			return NULL;
		}
	}

	if (eoffs) {
		ml->eoffs = eoffs;
		vaddr = (void *)FLOOR((ptr_t)data + size);
		epa = pmap_resolve(&srcmap->pmap, vaddr) & ~(SIZE_PAGE - 1);

		if ((boffs == 0) || (eoffs >= boffs)) {
			nep = vm_pageAlloc(SIZE_PAGE, PAGE_OWNER_APP);
			ml->ep = nep;
			if (nep == NULL) {
				return NULL;
			}
		}
		else {
			nep = nbp;
		}

		vaddr = vm_mmap(msg_common.kmap, NULL, NULL, SIZE_PAGE, PROT_READ | PROT_WRITE, VM_OBJ_PHYSMEM, epa, flags);
		ml->evaddr = vaddr;
		if (vaddr == NULL) {
			return NULL;
		}

		/* Map new page into destination address space */
		if (page_map(&dstmap->pmap, w + (n + ((boffs != 0) ? 1 : 0)) * SIZE_PAGE, nep->addr, (attr | PGHD_WRITE) & ~PGHD_USER) < 0) {
			return NULL;
		}

		hal_memcpy(w + (n + ((boffs != 0) ? 1 : 0)) * SIZE_PAGE, vaddr, eoffs);

		if (page_map(&dstmap->pmap, w + (n + ((boffs != 0) ? 1 : 0)) * SIZE_PAGE, nep->addr, attr) < 0) {
			return NULL;
		}
	}

	return (w + boffs);
}


static void msg_release(kmsg_t *kmsg)
{
	return;
	process_t *process;
	vm_map_t *map;

	if (kmsg->i.bp != NULL) {
		vm_pageFree(kmsg->i.bp);
		vm_munmap(msg_common.kmap, kmsg->i.bvaddr, SIZE_PAGE);
		kmsg->i.bp = NULL;
	}

	if (kmsg->i.eoffs != 0) {
		if (kmsg->i.ep != NULL) {
			vm_pageFree(kmsg->i.ep);
		}
		vm_munmap(msg_common.kmap, kmsg->i.evaddr, SIZE_PAGE);
		kmsg->i.eoffs = 0;
		kmsg->i.ep = NULL;
	}

	process = proc_current()->process;
	if (process != NULL) {
		map = process->mapp;
	}
	else {
		map = msg_common.kmap;
	}

	if (kmsg->i.w != NULL) {
		vm_munmap(map, kmsg->i.w, CEIL((ptr_t)kmsg->msg.i.data + kmsg->msg.i.size) - FLOOR((ptr_t)kmsg->msg.i.data));
		kmsg->i.w = NULL;
	}

	if (kmsg->o.bp != NULL) {
		vm_pageFree(kmsg->o.bp);
		vm_munmap(msg_common.kmap, kmsg->o.bvaddr, SIZE_PAGE);
		kmsg->o.bp = NULL;
	}

	if (kmsg->o.eoffs) {
		if (kmsg->o.ep != NULL) {
			vm_pageFree(kmsg->o.ep);
		}
		vm_munmap(msg_common.kmap, kmsg->o.evaddr, SIZE_PAGE);
		kmsg->o.eoffs = 0;
		kmsg->o.ep = NULL;
	}

	if (kmsg->o.w != NULL) {
		vm_munmap(map, kmsg->o.w, CEIL((ptr_t)kmsg->msg.o.data + kmsg->msg.o.size) - FLOOR((ptr_t)kmsg->msg.o.data));
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


int proc_send(u32 port, msg_t *msg)
{
	port_t *p;
	int err = EOK;
	kmsg_t kmsg;
	thread_t *sender;
	spinlock_ctx_t sc;

	/* TODO - check if msg pointer belongs to user vm_map */
	if (msg == NULL) {
		return -EINVAL;
	}

	p = proc_portGet(port);
	if (p == NULL) {
		return -EINVAL;
	}

	sender = proc_current();

	u64 now = csr_read(cycle);

	hal_memcpy(&kmsg.msg, msg, sizeof(msg_t));
	kmsg.src = sender->process;
	kmsg.threads = NULL;
	kmsg.state = msg_waiting;

	kmsg.msg.pid = (sender->process != NULL) ? process_getPid(sender->process) : 0;
	kmsg.msg.priority = sender->priority;

	msg_ipack(&kmsg);
	hal_memcpy(buffer, msg->i.data, msg->i.size);
	kmsg.msg.i.data = buffer;

	now = csr_read(cycle) - now;
	sendTimes[nsend].size = kmsg.msg.i.size;
	sendTimes[nsend].alignment = (ptr_t)kmsg.msg.i.data & (SIZE_PAGE - 1);
	sendTimes[nsend++].time = now;

	hal_spinlockSet(&p->spinlock, &sc);

	if (p->closed != 0) {
		err = -EINVAL;
	}
	else {
		LIST_ADD(&p->kmessages, &kmsg);
		proc_threadWakeup(&p->threads);

		while ((kmsg.state != msg_responded) && (kmsg.state != msg_rejected)) {

			err = proc_threadWaitInterruptible(&kmsg.threads, &p->spinlock, 0, &sc);

			if ((err != EOK) && (kmsg.state == msg_waiting)) {
				LIST_REMOVE(&p->kmessages, &kmsg);
				break;
			}
		}

		switch (kmsg.state) {
			case msg_responded:
				err = EOK; /* Don't report EINTR if we got the response already */
				break;
			case msg_rejected:
				err = -EINVAL;
				break;
			default:
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

	if (p->closed) {
		/* Port is being removed */
		if (kmsg != NULL) {
			kmsg->state = msg_rejected;
			LIST_REMOVE(&p->kmessages, kmsg);
			proc_threadWakeup(&kmsg->threads);
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

	if ((kmsg->msg.i.data >= (void *)kmsg->msg.i.raw) && (kmsg->msg.i.data < (void *)kmsg->msg.i.raw + sizeof(kmsg->msg.i.raw))) {
		ipacked = 1;
	}

	u64 now = csr_read(cycle);

	/* Map data in receiver space */
	/* Don't map if msg is packed */
	if (ipacked == 0) {
		// kmsg->msg.i.data = msg_map(0, kmsg, (void *)kmsg->msg.i.data, kmsg->msg.i.size, kmsg->src, proc_current()->process);
	}

	opacked = msg_opack(kmsg);
	if (opacked == 0) {
		kmsg->msg.o.data = msg_map(1, kmsg, kmsg->msg.o.data, kmsg->msg.o.size, kmsg->src, proc_current()->process);
	}

	if (((kmsg->msg.i.size != 0) && (kmsg->msg.i.data == NULL)) ||
			((kmsg->msg.o.size != 0) && (kmsg->msg.o.data == NULL)) ||
			(proc_portRidAlloc(p, kmsg) < 0)) {
		msg_release(kmsg);

		hal_spinlockSet(&p->spinlock, &sc);
		kmsg->state = msg_rejected;
		proc_threadWakeup(&kmsg->threads);
		hal_spinlockClear(&p->spinlock, &sc);

		port_put(p, 0);

		return -ENOMEM;
	}

	*rid = lib_idtreeId(&kmsg->idlinkage);

	void *ibuf = msg->i.data;

	hal_memcpy(msg, &kmsg->msg, sizeof(*msg));
	hal_memcpy(ibuf, buffer, kmsg->msg.i.size);
	msg->i.data = ibuf;

	now = csr_read(cycle) - now;
	recvTimes[nrecv].size = kmsg->msg.i.size;
	recvTimes[nrecv].alignment = (ptr_t)kmsg->msg.i.data & (SIZE_PAGE - 1);
	recvTimes[nrecv++].time = now;

	if (ipacked != 0) {
		// msg->i.data = msg->i.raw + (kmsg->msg.i.data - (void *)kmsg->msg.i.raw);
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
	size_t s = 0;
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

	u64 now = csr_read(cycle);

	/* Copy shadow pages */
	if (kmsg->i.bp != NULL) {
		hal_memcpy(kmsg->i.bvaddr + kmsg->i.boffs, kmsg->i.w + kmsg->i.boffs, min(SIZE_PAGE - kmsg->i.boffs, kmsg->msg.i.size));
	}

	if (kmsg->i.eoffs) {
		hal_memcpy(kmsg->i.evaddr, kmsg->i.w + kmsg->i.boffs + kmsg->msg.i.size - kmsg->i.eoffs, kmsg->i.eoffs);
	}

	if (kmsg->o.bp != NULL) {
		hal_memcpy(kmsg->o.bvaddr + kmsg->o.boffs, kmsg->o.w + kmsg->o.boffs, min(SIZE_PAGE - kmsg->o.boffs, kmsg->msg.o.size));
	}

	if (kmsg->o.eoffs) {
		hal_memcpy(kmsg->o.evaddr, kmsg->o.w + kmsg->o.boffs + kmsg->msg.o.size - kmsg->o.eoffs, kmsg->o.eoffs);
	}

	msg_release(kmsg);

	hal_memcpy(kmsg->msg.o.raw, msg->o.raw, sizeof(msg->o.raw));
	kmsg->msg.o.err = msg->o.err;

	now = csr_read(cycle) - now;
	respondTimes[nrespond].size = kmsg->msg.i.size;
	respondTimes[nrespond].alignment = (ptr_t)kmsg->msg.i.data & (SIZE_PAGE - 1);
	respondTimes[nrespond++].time = now;

	hal_spinlockSet(&p->spinlock, &sc);
	kmsg->state = msg_responded;
	kmsg->src = proc_current()->process;
	proc_threadWakeup(&kmsg->threads);
	hal_spinlockClear(&p->spinlock, &sc);
	hal_cpuReschedule(NULL, NULL);

	port_put(p, 0);

	return s;
}


void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
	nrecv = 0;
	nrespond = 0;
	nsend = 0;
	msg_common.kmap = kmap;
	msg_common.kernel = kernel;
}
