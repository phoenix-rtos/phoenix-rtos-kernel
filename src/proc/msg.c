/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Ports and messages
 *
 * Copyright 2017 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk
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


typedef struct _port_t {
	rbnode_t linkage;
	struct _port_t *next;
	struct _port_t *prev;

	u32 id;
	u32 lmaxgap;
	u32 rmaxgap;
	kmsg_t *kmessages;
	kmsg_t *received;
	process_t *owner;
	int refs, closed;

	spinlock_t spinlock;
	thread_t *threads;
	msg_t *current;
} port_t;


struct {
	rbtree_t tree;
	lock_t port_lock;

	vm_map_t *kmap;
	vm_object_t *kernel;
} msg_common;


static int ports_cmp(rbnode_t *n1, rbnode_t *n2)
{
	port_t *p1 = lib_treeof(port_t, linkage, n1);
	port_t *p2 = lib_treeof(port_t, linkage, n2);

	return (p1->id - p2->id);
}


static int ports_gapcmp(rbnode_t *n1, rbnode_t *n2)
{
	port_t *p1 = lib_treeof(port_t, linkage, n1);
	port_t *p2 = lib_treeof(port_t, linkage, n2);
	rbnode_t *child = NULL;
	int ret = 1;

	if (p1->lmaxgap > 0 && p1->rmaxgap > 0) {
		if (p2->id > p1->id) {
			child = n1->right;
			ret = -1;
		}
		else {
			child = n1->left;
			ret = 1;
		}
	}
	else if (p1->lmaxgap > 0) {
		child = n1->left;
		ret = 1;
	}
	else if (p1->rmaxgap > 0) {
		child = n1->right;
		ret = -1;
	}

	if (child == NULL)
		return 0;

	return ret;
}


static void ports_augment(rbnode_t *node)
{
	rbnode_t *it;
	rbnode_t *parent = node->parent;
	port_t *n = lib_treeof(port_t, linkage, node);
	port_t *p = lib_treeof(port_t, linkage, parent);
	port_t *pp = (parent != NULL) ? lib_treeof(port_t, linkage, parent->parent) : NULL;

	if (node->left == NULL) {
		if (parent != NULL && parent->right == node)
			n->lmaxgap = n->id - p->id - 1;
		else if (parent != NULL && parent->parent != NULL && parent->parent->right == parent)
			n->lmaxgap = n->id - pp->id - 1;
		else
			n->lmaxgap = n->id - 0;
	}
	else {
		port_t *l = lib_treeof(port_t, linkage, node->left);
		n->lmaxgap = max(l->lmaxgap, l->rmaxgap);
	}

	if (node->right == NULL) {
		if (parent != NULL && parent->left == node)
			n->rmaxgap = p->id - n->id - 1;
		else if (parent != NULL && parent->parent != NULL && parent->parent->left == parent)
			n->rmaxgap = pp->id - n->id - 1;
		else
			n->rmaxgap = (u32)-1 - n->id - 1;
	}
	else {
		port_t *r = lib_treeof(port_t, linkage, node->right);
		n->rmaxgap = max(r->lmaxgap, r->rmaxgap);
	}

	for (it = node; it->parent != NULL; it = it->parent) {
		n = lib_treeof(port_t, linkage, it);
		p = lib_treeof(port_t, linkage, it->parent);

		if (it->parent->left == it)
			p->lmaxgap = max(n->lmaxgap, n->rmaxgap);
		else
			p->rmaxgap = max(n->lmaxgap, n->rmaxgap);
	}
}


static int _proc_portAlloc(u32 *id)
{
	port_t *p;
	port_t t;

	if (msg_common.tree.root == NULL) {
		*id = 0;
		return EOK;
	}

	t.id = 0;
	p = lib_treeof(port_t, linkage, lib_rbFindEx(msg_common.tree.root, &t.linkage, ports_gapcmp));
	if (p != NULL) {
		if (p->lmaxgap > 0)
			*id = p->id - 1;
		else if (p->rmaxgap > 0)
			*id = p->id + 1;
		else
			return -ENOMEM;

		return EOK;
	}

	return -EINVAL;
}


static port_t *proc_portGet(u32 id)
{
	port_t *port;
	port_t t;

	t.id = id;

	proc_lockSet(&msg_common.port_lock);
	port = lib_treeof(port_t, linkage, lib_rbFind(&msg_common.tree, &t.linkage));
	if (port != NULL) {
		hal_spinlockSet(&port->spinlock);
		port->refs++;
		hal_spinlockClear(&port->spinlock);
	}
	proc_lockClear(&msg_common.port_lock);

	return port;
}


static void port_put(port_t *p, int destroy)
{
	proc_lockSet(&msg_common.port_lock);
	hal_spinlockSet(&p->spinlock);
	p->refs--;

	if (destroy)
		p->closed = 1;

	if (p->refs) {
		if (destroy) {
			/* Wake receivers up */
			while (p->threads != NULL && p->threads != (void *)-1)
				proc_threadWakeup(&p->threads);
		}

		hal_spinlockClear(&p->spinlock);
		proc_lockClear(&msg_common.port_lock);
		return;
	}

	hal_spinlockClear(&p->spinlock);
	lib_rbRemove(&msg_common.tree, &p->linkage);
	proc_lockClear(&msg_common.port_lock);

	proc_lockSet(&p->owner->lock);
	if (p->next != NULL)
		LIST_REMOVE(&p->owner->ports, p);
	proc_lockClear(&p->owner->lock);

	hal_spinlockDestroy(&p->spinlock);
	vm_kfree(p);
}


int proc_portCreate(u32 *id)
{
	port_t *port;
	thread_t *curr;
	process_t *proc = NULL;


	if ((port = vm_kmalloc(sizeof(port_t))) == NULL)
		return -ENOMEM;

	proc_lockSet(&msg_common.port_lock);
	if (_proc_portAlloc(&port->id) != EOK) {
		proc_lockClear(&msg_common.port_lock);
		vm_kfree(port);
		return -EINVAL;
	}

	lib_rbInsert(&msg_common.tree, &port->linkage);

	port->kmessages = NULL;
	port->received = NULL;
	hal_spinlockCreate(&port->spinlock, "port.spinlock");

	port->threads = NULL;
	port->current = NULL;
	port->refs = 1;
	port->closed = 0;

	*id = port->id;
	proc_lockClear(&msg_common.port_lock);

	if ((curr = proc_current()) != NULL && (proc = curr->process) != NULL) {
		proc_lockSet(&proc->lock);
		LIST_ADD((&proc->ports), port);
		proc_lockClear(&proc->lock);
	}

	port->owner = proc;

	proc_lockSet(&msg_common.port_lock);
	lib_rbInsert(&msg_common.tree, &port->linkage);
	proc_lockClear(&msg_common.port_lock);

	return EOK;
}


void proc_portDestroy(u32 port)
{
	port_t *p;
	thread_t *curr;
	process_t *proc = NULL;

	if ((p = proc_portGet(port)) == NULL)
		return;

	if (p->closed) {
		port_put(p, 0);
		return;
	}

	if ((curr = proc_current()) != NULL && (proc = curr->process) != NULL) {
		if (p->owner != proc) {
			port_put(p, 0);
			return;
		}
	}

	port_put(p, 0);
	port_put(p, 1);
}


void *msg_map(int dir, kmsg_t *kmsg, void *data, size_t size, process_t *from, process_t *to)
{
	void *w = NULL, *vaddr;
	u64 boffs, eoffs;
	unsigned int n = 0, i, attr;
	page_t *ep = NULL, *nep = NULL, *bp = NULL, *nbp = NULL, *p;
	vm_map_t *srcmap, *dstmap;
	struct _kmsg_layout_t *ml = dir ? &kmsg->o : &kmsg->i;

	if ((size == 0) || (data == NULL))
		return EOK;

	attr = PGHD_PRESENT;

	if (dir)
		attr |= PGHD_WRITE;

	if (to != NULL)
		attr |= PGHD_USER;

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

	if (srcmap == dstmap)
		return data;

	if ((ml->w = w = vm_mapFind(dstmap, (void *)0, (!!boffs + !!eoffs + n) * SIZE_PAGE, MAP_NOINHERIT)) == NULL)
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


int proc_send(u32 port, kmsg_t *kmsg)
{
	port_t *p;
	int responded = 0;
	int err = EOK;


	if ((p = proc_portGet(port)) == NULL)
		return -EINVAL;

	kmsg->threads = NULL;
	kmsg->src = proc_current()->process;
	kmsg->responded = 0;

	hal_spinlockSet(&p->spinlock);

	if (p->closed) {
		err = -EINVAL;
	}
	else {
		LIST_ADD(&p->kmessages, kmsg);

		proc_threadWakeup(&p->threads);

		while (!(responded = kmsg->responded))
			err = proc_threadWait(&kmsg->threads, &p->spinlock, 0);
	}

	hal_spinlockClear(&p->spinlock);
	port_put(p, 0);

	return responded < 0 ? -EINVAL : err;
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


int proc_recv(u32 port, kmsg_t **kmsg, unsigned int *rid)
{
	port_t *p;
	int closed;

	if ((p = proc_portGet(port)) == NULL)
		return -EINVAL;

	proc_threadUnprotect();
	hal_spinlockSet(&p->spinlock);

	while (p->kmessages == NULL && !p->closed)
		proc_threadWait(&p->threads, &p->spinlock, 0);

	(*kmsg) = p->kmessages;

	if (p->closed) {
		/* Port is being removed */
		if (*kmsg != NULL) {
			(*kmsg)->responded = -1;
			LIST_REMOVE(&p->kmessages, (*kmsg));
			proc_threadWakeup(&(*kmsg)->threads);
		}

		hal_spinlockClear(&p->spinlock);
		proc_threadProtect();
		port_put(p, 0);
		return -EINVAL;
	}

	LIST_REMOVE(&p->kmessages, (*kmsg));
	LIST_ADD(&p->received, (*kmsg));
	hal_spinlockClear(&p->spinlock);
	proc_threadProtect();

	/* (MOD) */
	(*rid) = (unsigned long)(*kmsg);

	(*kmsg)->i.bvaddr = NULL;
	(*kmsg)->i.boffs = 0;
	(*kmsg)->i.w = NULL;
	(*kmsg)->i.bp = NULL;
	(*kmsg)->i.evaddr = NULL;
	(*kmsg)->i.eoffs = 0;
	(*kmsg)->i.ep = NULL;

	(*kmsg)->o.bvaddr = NULL;
	(*kmsg)->o.boffs = 0;
	(*kmsg)->o.w = NULL;
	(*kmsg)->o.bp = NULL;
	(*kmsg)->o.evaddr = NULL;
	(*kmsg)->o.eoffs = 0;
	(*kmsg)->o.ep = NULL;

	/* Map data in receiver space */
	(*kmsg)->msg.i.data = msg_map(0, (*kmsg), (*kmsg)->msg.i.data, (*kmsg)->msg.i.size, (*kmsg)->src, proc_current()->process);
	(*kmsg)->msg.o.data = msg_map(1, (*kmsg), (*kmsg)->msg.o.data, (*kmsg)->msg.o.size, (*kmsg)->src, proc_current()->process);

	if (((*kmsg)->msg.i.size && (*kmsg)->msg.i.data == NULL) ||
		((*kmsg)->msg.o.size && (*kmsg)->msg.o.data == NULL) ||
		p->closed) {
		closed = p->closed;
		msg_release(*kmsg);

		hal_spinlockSet(&p->spinlock);
		LIST_REMOVE(&p->received, (*kmsg));
		(*kmsg)->responded = -1;
		proc_threadWakeup(&(*kmsg)->threads);
		hal_spinlockClear(&p->spinlock);

		port_put(p, 0);

		return closed ? -EINVAL : -ENOMEM;
	}

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
	kmsg->responded = 1;
	kmsg->src = proc_current()->process;
	LIST_REMOVE(&p->received, kmsg);
	proc_threadWakeup(&kmsg->threads);
	hal_spinlockClear(&p->spinlock);
	port_put(p, 0);

	return s;
}


void proc_portsDestroy(process_t *proc)
{
	port_t *p;

	while (proc_lockSet(&proc->lock), (p = proc->ports) != NULL) {
		LIST_REMOVE(&proc->ports, p);
		proc_lockClear(&proc->lock);
		port_put(p, 1);
	}
	proc_lockClear(&proc->lock);
}


void _msg_init(vm_map_t *kmap, vm_object_t *kernel)
{
	lib_rbInit(&msg_common.tree, ports_cmp, ports_augment);
	proc_lockInit(&msg_common.port_lock);

	msg_common.kmap = kmap;
	msg_common.kernel = kernel;
}
