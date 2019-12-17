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
#include "file.h"
#include "ports.h"

#define MAX_SPECIFIED_PORT 10000

struct {
	rbtree_t tree;
	lock_t lock;
	unsigned int freshId;
} port_common;


static int obdesc_cmp(rbnode_t *n1, rbnode_t *n2)
{
	obdes_t *d1 = lib_treeof(obdes_t, linkage, n1);
	obdes_t *d2 = lib_treeof(obdes_t, linkage, n2);
	return (d1->id > d2->id) - (d1->id < d2->id);
}


static int ports_cmp(rbnode_t *n1, rbnode_t *n2)
{
	port_t *p1 = lib_treeof(port_t, linkage, n1);
	port_t *p2 = lib_treeof(port_t, linkage, n2);

	return (p1->id > p2->id) - (p1->id < p2->id);
}


obdes_t *port_obdesGet(port_t *port, id_t id)
{
	obdes_t *od, t;
	t.id = id;

	proc_lockSet(&port->odlock);
	if ((od = lib_treeof(obdes_t, linkage, lib_rbFind(&port->obdes, &t))) == NULL) {
		if ((od = vm_kmalloc(sizeof(*od))) != NULL) {
			od->refs = 1;
			od->id = id;
			lib_rbInsert(&port->obdes, &od->linkage);
			od->queue = NULL;
			port_ref(port);
			od->port = port;
		}
	}
	else {
		od->refs++;
	}
	proc_lockClear(&port->odlock);

	return od;
}


void port_obdesPut(obdes_t *obdes)
{
	port_t *port = obdes->port;
	int refs;

	proc_lockSet(&port->odlock);
	if (!(refs = --obdes->refs))
		lib_rbRemove(&port->obdes, &obdes->linkage);
	proc_lockClear(&port->odlock);

	if (!refs) {
		if (obdes->queue != NULL)
			lib_printf("error: obdes queue not empty despite no references\n");

		port_put(port);
		vm_kfree(obdes);
	}
}


int port_event(port_t *port, id_t id, int events)
{
	obdes_t *od, t;
	t.id = id;

	proc_lockSet(&port->odlock);
	if ((od = lib_treeof(obdes_t, linkage, lib_rbFind(&port->obdes, &t))) != NULL)
		poll_signal(&od->queue, events);
	proc_lockClear(&port->odlock);
	return EOK;
}


int proc_event(int portfd, id_t id, int events)
{
	process_t *process = proc_current()->process;
	file_t *file;
	int retval;

	if ((file = file_get(process, portfd)) == NULL)
		return -EBADF;

	if (file->type != ftPort) {
		file_put(file);
		return -EBADF;
	}

	retval = port_event(file->port, id, events);
	file_put(file);
	return retval;
}


void port_ref(port_t *port)
{
	proc_lockSet(&port_common.lock);
	port->refs++;
	proc_lockClear(&port_common.lock);
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

	if (id > MAX_SPECIFIED_PORT)
		return -EINVAL;

	if ((p = vm_kmalloc(sizeof(port_t))) == NULL)
		return -ENOMEM;

	p->id = id;
	p->kmessages = NULL;
	p->threads = NULL;
	p->refs = 1;
	hal_spinlockCreate(&p->spinlock, "port.spinlock");
	lib_rbInit(&p->obdes, obdesc_cmp, NULL);
	proc_lockInit(&p->odlock);

	proc_lockSet(&port_common.lock);
	if (id == 0) {
		id = port_common.freshId++;
		p->id = id;
	}

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


static int port_release(file_t *file)
{
	port_put(file->port);
	return EOK;
}


int proc_portCreate(u32 id)
{
	process_t *process = proc_current()->process;
	int err;
	file_t *file;

	if ((file = file_alloc()) == NULL)
		return -ENOMEM;

	file->type = ftPort;

	if ((err = port_create(&file->port, id)) == EOK) {
		if ((err = fd_new(process, 0, 0, file)) < 0)
			file_put(file);
	}
	else {
		file_destroy(file);
	}

	return err;
}


int proc_portGet(u32 id)
{
	process_t *process = proc_current()->process;
	int err;
	file_t *file;

	if ((file = file_alloc()) == NULL)
		return -ENOMEM;

	file->type = ftPort;

	if ((file->port = port_get(id)) != NULL) {
		if ((err = fd_new(process, 0, 0, file)) < 0)
			file_put(file);
	}
	else {
		err = -ENXIO;
		file_destroy(file);
	}

	return err;
}


void _port_init(void)
{
	port_common.freshId = MAX_SPECIFIED_PORT + 1;
	lib_rbInit(&port_common.tree, ports_cmp, NULL);
	proc_lockInit(&port_common.lock);
}
