/*
 * Phoenix-RTOS
 *
 * Operating system kernel
 *
 * Ports
 *
 * Copyright 2017, 2018 Phoenix Systems
 * Author: Jakub Sejdak, Pawel Pisarczyk, Aleksander Kaminski, Jan Sikorski
 *
 * This file is part of Phoenix-RTOS.
 *
 * %LICENSE%
 */

#include "ports.h"


struct {
	rbtree_t tree;
	lock_t port_lock;
} port_common;


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
	port_t *n = lib_treeof(port_t, linkage, node);
	port_t *p = n, *r, *l;

	if (node->left == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(port_t, linkage, it->parent);
			if (it->parent->right == it)
				break;
		}

		n->lmaxgap = (n->id <= p->id) ? n->id : n->id - p->id - 1;
	}
	else {
		l = lib_treeof(port_t, linkage, node->left);
		n->lmaxgap = max(l->lmaxgap, l->rmaxgap);
	}

	if (node->right == NULL) {
		for (it = node; it->parent != NULL; it = it->parent) {
			p = lib_treeof(port_t, linkage, it->parent);
			if (it->parent->left == it)
				break;
		}

		n->rmaxgap = (n->id >= p->id) ? (unsigned)-1 - n->id - 1 : p->id - n->id - 1;
	}
	else {
		r = lib_treeof(port_t, linkage, node->right);
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

	if (port_common.tree.root == NULL) {
		*id = 0;
		return EOK;
	}

	t.id = 0;
	p = lib_treeof(port_t, linkage, lib_rbFindEx(port_common.tree.root, &t.linkage, ports_gapcmp));
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


port_t *proc_portGet(u32 id)
{
	port_t *port;
	port_t t;

	t.id = id;

	proc_lockSet(&port_common.port_lock);
	port = lib_treeof(port_t, linkage, lib_rbFind(&port_common.tree, &t.linkage));
	if (port != NULL) {
		hal_spinlockSet(&port->spinlock);
		port->refs++;
		hal_spinlockClear(&port->spinlock);
	}
	proc_lockClear(&port_common.port_lock);

	return port;
}


void port_put(port_t *p, int destroy)
{
	proc_lockSet(&port_common.port_lock);
	hal_spinlockSet(&p->spinlock);
	p->refs--;

	if (destroy)
		p->closed = 1;

	if (p->refs) {
		if (destroy)
			/* Wake receivers up */
			proc_threadBroadcast(&p->threads);

		hal_spinlockClear(&p->spinlock);
		proc_lockClear(&port_common.port_lock);
		return;
	}

	hal_spinlockClear(&p->spinlock);
	lib_rbRemove(&port_common.tree, &p->linkage);
	proc_lockClear(&port_common.port_lock);

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

	proc_lockSet(&port_common.port_lock);
	if (_proc_portAlloc(&port->id) != EOK) {
		proc_lockClear(&port_common.port_lock);
		vm_kfree(port);
		return -EINVAL;
	}

	lib_rbInsert(&port_common.tree, &port->linkage);

	port->kmessages = NULL;
	hal_spinlockCreate(&port->spinlock, "port.spinlock");

	port->threads = NULL;
	port->current = NULL;
	port->refs = 1;
	port->closed = 0;

	*id = port->id;
	proc_lockClear(&port_common.port_lock);

	if ((curr = proc_current()) != NULL && (proc = curr->process) != NULL) {
		proc_lockSet(&proc->lock);
		LIST_ADD((&proc->ports), port);
		proc_lockClear(&proc->lock);
	}

	port->owner = proc;

	proc_lockSet(&port_common.port_lock);
	lib_rbInsert(&port_common.tree, &port->linkage);
	proc_lockClear(&port_common.port_lock);

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


void _port_init(void)
{
	lib_rbInit(&port_common.tree, ports_cmp, ports_augment);
	proc_lockInit(&port_common.port_lock);
}
