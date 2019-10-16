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

#include "../lib/lib.h"
#include "ports.h"


struct {
	rbtree_t tree;
	lock_t lock;
} port_common;


static int ports_cmp(rbnode_t *n1, rbnode_t *n2)
{
	port_t *p1 = lib_treeof(port_t, linkage, n1);
	port_t *p2 = lib_treeof(port_t, linkage, n2);

	return (p1->id > p2->id) - (p1->id < p2->id);
}


port_t *port_get(u32 id)
{
	port_t *port;
	port_t t;

	t.id = id;

	proc_lockSet(&port_common.lock);
	if ((port = lib_treeof(port_t, linkage, lib_rbFind(&port_common.tree, &t.linkage))) != NULL)
		port->refs++;
	proc_lockClear(&port_common.lock);

	return port;
}


void port_put(port_t *port)
{
	int refs;

	proc_lockSet(&port_common.lock);
	if (!(refs = --port->refs))
		lib_rbRemove(&port_common.tree, &port->linkage);
	proc_lockClear(&port_common.lock);

	if (!refs) {
		hal_spinlockDestroy(&port->spinlock);
		vm_kfree(port);
	}
}


int port_create(port_t **port, u32 id)
{
	port_t *p;
	int err;

	if ((p = vm_kmalloc(sizeof(port_t))) == NULL)
		return -ENOMEM;

	p->id = id;
	p->kmessages = NULL;
	p->threads = NULL;
	p->refs = 1;
	hal_spinlockCreate(&p->spinlock, "port.spinlock");

	proc_lockSet(&port_common.lock);
	err = lib_rbInsert(&port_common.tree, &p->linkage);
	proc_lockClear(&port_common.lock);

	if (err < 0) {
		hal_spinlockDestroy(&p->spinlock);
		vm_kfree(p);
		return -EEXIST;
	}

	*port = p;
	return EOK;
}


void _port_init(void)
{
	lib_rbInit(&port_common.tree, ports_cmp, NULL);
	proc_lockInit(&port_common.lock);
}
